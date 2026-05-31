import os
import sys
import json
import struct
import hashlib
import numpy as np
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad


CYPHERPUF_STATIC_AES_KEY = bytes([
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
    0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A
])


def derive_key_from_puf_simulation(raw_puf_key):
    key_hash = hashlib.sha256(raw_puf_key).digest()
    return key_hash


def parse_encrypted_binary(file_path):
    with open(file_path, 'rb') as f:
        data = f.read()

    offset = 0

    magic = data[offset:offset + 4]
    offset += 4
    if magic != b'CPFE':
        raise ValueError(f"Gecersiz magic number: {magic}, beklenen: CPFE")

    version_major = struct.unpack('<B', data[offset:offset + 1])[0]
    offset += 1
    version_minor = struct.unpack('<B', data[offset:offset + 1])[0]
    offset += 1

    mode_byte = struct.unpack('<B', data[offset:offset + 1])[0]
    offset += 1

    if mode_byte == 0x01:
        encryption_mode = 'GCM'
    elif mode_byte == 0x02:
        encryption_mode = 'CBC'
    else:
        raise ValueError(f"Gecersiz sifreleme modu: 0x{mode_byte:02X}")

    reserved = struct.unpack('<B', data[offset:offset + 1])[0]
    offset += 1

    metadata_length = struct.unpack('<I', data[offset:offset + 4])[0]
    offset += 4

    metadata_json = data[offset:offset + metadata_length].decode('utf-8')
    metadata = json.loads(metadata_json)
    offset += metadata_length

    if encryption_mode == 'GCM':
        nonce_length = struct.unpack('<B', data[offset:offset + 1])[0]
        offset += 1
        nonce = data[offset:offset + nonce_length]
        offset += nonce_length

        tag_length = struct.unpack('<B', data[offset:offset + 1])[0]
        offset += 1
        auth_tag = data[offset:offset + tag_length]
        offset += tag_length
    elif encryption_mode == 'CBC':
        iv_length = struct.unpack('<B', data[offset:offset + 1])[0]
        offset += 1
        nonce = data[offset:offset + iv_length]
        offset += iv_length
        auth_tag = b''

    ciphertext_length = struct.unpack('<Q', data[offset:offset + 8])[0]
    offset += 8

    ciphertext = data[offset:offset + ciphertext_length]
    offset += ciphertext_length

    parsed = {
        'magic': magic.decode('ascii'),
        'version': f'{version_major}.{version_minor}',
        'encryption_mode': encryption_mode,
        'metadata': metadata,
        'nonce': nonce,
        'auth_tag': auth_tag,
        'ciphertext': ciphertext,
        'total_file_size': len(data),
        'header_size': offset - len(ciphertext),
        'ciphertext_size': len(ciphertext)
    }

    return parsed


def decrypt_data(ciphertext, nonce, auth_tag, aes_key, mode='GCM'):
    if mode == 'GCM':
        cipher = AES.new(aes_key, AES.MODE_GCM, nonce=nonce)
        plaintext = cipher.decrypt_and_verify(ciphertext, auth_tag)
    elif mode == 'CBC':
        cipher = AES.new(aes_key, AES.MODE_CBC, iv=nonce)
        decrypted_padded = cipher.decrypt(ciphertext)
        plaintext = unpad(decrypted_padded, AES.block_size)
    else:
        raise ValueError(f"Desteklenmeyen sifreleme modu: {mode}")

    return plaintext


def parse_weight_binary(plaintext_data):
    offset = 0

    magic = plaintext_data[offset:offset + 4]
    offset += 4
    if magic != b'CPUF':
        raise ValueError(f"Gecersiz agirlik magic number: {magic}, beklenen: CPUF")

    version_major = struct.unpack('<B', plaintext_data[offset:offset + 1])[0]
    offset += 1
    version_minor = struct.unpack('<B', plaintext_data[offset:offset + 1])[0]
    offset += 1

    total_arrays = struct.unpack('<I', plaintext_data[offset:offset + 4])[0]
    offset += 4

    total_elements = struct.unpack('<Q', plaintext_data[offset:offset + 8])[0]
    offset += 8

    reserved = plaintext_data[offset:offset + 16]
    offset += 16

    array_shapes = []
    array_sizes = []
    for _ in range(total_arrays):
        ndim = struct.unpack('<B', plaintext_data[offset:offset + 1])[0]
        offset += 1
        shape = []
        for _ in range(ndim):
            dim = struct.unpack('<I', plaintext_data[offset:offset + 4])[0]
            offset += 4
            shape.append(dim)
        array_shapes.append(tuple(shape))

        num_elements = struct.unpack('<I', plaintext_data[offset:offset + 4])[0]
        offset += 4
        size_bytes = struct.unpack('<I', plaintext_data[offset:offset + 4])[0]
        offset += 4
        array_sizes.append((num_elements, size_bytes))

    weight_arrays = []
    for i in range(total_arrays):
        num_elements = array_sizes[i][0]
        byte_count = num_elements * 4
        float_data = np.frombuffer(
            plaintext_data[offset:offset + byte_count],
            dtype=np.float32
        )
        weight_array = float_data.reshape(array_shapes[i])
        weight_arrays.append(weight_array)
        offset += byte_count

    parsed_weights = {
        'magic': magic.decode('ascii'),
        'version': f'{version_major}.{version_minor}',
        'total_arrays': total_arrays,
        'total_elements': total_elements,
        'array_shapes': array_shapes,
        'weight_arrays': weight_arrays
    }

    return parsed_weights


def compare_with_original(decrypted_weights, original_weights_path):
    original_data = np.load(original_weights_path)

    original_arrays = [original_data[key] for key in original_data.files]
    decrypted_arrays = decrypted_weights['weight_arrays']

    if len(original_arrays) != len(decrypted_arrays):
        return False, f"Dizi sayisi uyusmuyor: orijinal={len(original_arrays)}, cozumlenen={len(decrypted_arrays)}"

    for i in range(len(original_arrays)):
        if original_arrays[i].shape != decrypted_arrays[i].shape:
            return False, f"Dizi {i} sekil uyusmuyor: orijinal={original_arrays[i].shape}, cozumlenen={decrypted_arrays[i].shape}"

        if not np.allclose(original_arrays[i], decrypted_arrays[i], rtol=1e-5, atol=1e-7):
            max_diff = np.max(np.abs(original_arrays[i] - decrypted_arrays[i]))
            return False, f"Dizi {i} deger uyusmuyor: max fark={max_diff}"

    return True, "Tum diziler basariyla dogrulandi."


def verify_encryption():
    base_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'output')
    export_dir = os.path.join(base_dir, 'exported_weights')
    encrypt_dir = os.path.join(base_dir, 'encrypted_weights')

    encrypted_file = os.path.join(encrypt_dir, 'cypherpuf_encrypted_weights.bin')
    original_weights_bin = os.path.join(export_dir, 'cypherpuf_weights.bin')
    original_weights_npz = os.path.join(export_dir, 'numpy_weights', 'all_weights_combined.npz')

    print("=" * 70)
    print("CypherPUF - Faz 1: Uctan Uca Sifreleme Dogrulama")
    print("Gelistirici: Arda Mecik")
    print("=" * 70)

    test_results = []

    print("\n" + "-" * 70)
    print("TEST 1: Sifreli dosya yapisini ayristirma (parsing)")
    print("-" * 70)

    if not os.path.exists(encrypted_file):
        print(f"  HATA: Sifreli dosya bulunamadi: {encrypted_file}")
        print("  Lutfen once encrypt_weights.py betigini calistirin.")
        sys.exit(1)

    try:
        parsed = parse_encrypted_binary(encrypted_file)
        print(f"  Magic Number     : {parsed['magic']}")
        print(f"  Versiyon         : {parsed['version']}")
        print(f"  Sifreleme Modu   : {parsed['encryption_mode']}")
        print(f"  Dosya Boyutu     : {parsed['total_file_size']:,} byte")
        print(f"  Header Boyutu    : {parsed['header_size']:,} byte")
        print(f"  Sifreli Boyut    : {parsed['ciphertext_size']:,} byte")
        print(f"  Nonce Uzunlugu   : {len(parsed['nonce'])} byte")
        if parsed['auth_tag']:
            print(f"  Auth Tag Uzunlugu: {len(parsed['auth_tag'])} byte")
        print(f"  SONUC: BASARILI")
        test_results.append(('Dosya Parsing', True, 'Basarili'))
    except Exception as e:
        print(f"  SONUC: BASARISIZ - {str(e)}")
        test_results.append(('Dosya Parsing', False, str(e)))
        sys.exit(1)

    print("\n" + "-" * 70)
    print("TEST 2: AES-256 Sifre Cozumleme (Decryption)")
    print("-" * 70)

    try:
        aes_key = derive_key_from_puf_simulation(CYPHERPUF_STATIC_AES_KEY)
        print(f"  AES Anahtari (hex): {aes_key.hex()}")

        decrypted_data = decrypt_data(
            parsed['ciphertext'],
            parsed['nonce'],
            parsed['auth_tag'],
            aes_key,
            mode=parsed['encryption_mode']
        )
        print(f"  Cozumlenen boyut : {len(decrypted_data):,} byte")
        print(f"  SONUC: BASARILI")
        test_results.append(('AES Decryption', True, 'Basarili'))
    except Exception as e:
        print(f"  SONUC: BASARISIZ - {str(e)}")
        test_results.append(('AES Decryption', False, str(e)))
        sys.exit(1)

    print("\n" + "-" * 70)
    print("TEST 3: Orijinal ikili veri ile karsilastirma")
    print("-" * 70)

    if os.path.exists(original_weights_bin):
        with open(original_weights_bin, 'rb') as f:
            original_binary = f.read()

        if decrypted_data == original_binary:
            print(f"  Orijinal boyut   : {len(original_binary):,} byte")
            print(f"  Cozumlenen boyut : {len(decrypted_data):,} byte")
            print(f"  Byte-byte eslesme: EVET")
            print(f"  SONUC: BASARILI")
            test_results.append(('Binary Karsilastirma', True, 'Byte-byte eslesme'))
        else:
            print(f"  SONUC: BASARISIZ - Veriler eslesmedi")
            test_results.append(('Binary Karsilastirma', False, 'Veriler eslesmedi'))
    else:
        print(f"  ATLANDI: Orijinal dosya bulunamadi: {original_weights_bin}")
        test_results.append(('Binary Karsilastirma', None, 'Orijinal dosya bulunamadi'))

    print("\n" + "-" * 70)
    print("TEST 4: Agirlik dizilerini ayristirma ve dogrulama")
    print("-" * 70)

    try:
        decrypted_weights = parse_weight_binary(decrypted_data)
        print(f"  Magic Number     : {decrypted_weights['magic']}")
        print(f"  Versiyon         : {decrypted_weights['version']}")
        print(f"  Toplam dizi      : {decrypted_weights['total_arrays']}")
        print(f"  Toplam eleman    : {decrypted_weights['total_elements']:,}")

        for i, (shape, arr) in enumerate(zip(decrypted_weights['array_shapes'], decrypted_weights['weight_arrays'])):
            print(f"  Dizi {i:3d}: Sekil={str(shape):25s} | Min={np.min(arr):+.6f} | Max={np.max(arr):+.6f} | Mean={np.mean(arr):+.6f}")

        print(f"  SONUC: BASARILI")
        test_results.append(('Weight Parsing', True, 'Basarili'))
    except Exception as e:
        print(f"  SONUC: BASARISIZ - {str(e)}")
        test_results.append(('Weight Parsing', False, str(e)))

    print("\n" + "-" * 70)
    print("TEST 5: NumPy orijinal agirliklarla karsilastirma")
    print("-" * 70)

    if os.path.exists(original_weights_npz):
        try:
            match_result, match_message = compare_with_original(
                decrypted_weights, original_weights_npz
            )
            if match_result:
                print(f"  Karsilastirma    : {match_message}")
                print(f"  SONUC: BASARILI")
                test_results.append(('NumPy Karsilastirma', True, match_message))
            else:
                print(f"  Karsilastirma    : {match_message}")
                print(f"  SONUC: BASARISIZ")
                test_results.append(('NumPy Karsilastirma', False, match_message))
        except Exception as e:
            print(f"  SONUC: BASARISIZ - {str(e)}")
            test_results.append(('NumPy Karsilastirma', False, str(e)))
    else:
        print(f"  ATLANDI: NPZ dosyasi bulunamadi: {original_weights_npz}")
        test_results.append(('NumPy Karsilastirma', None, 'NPZ dosyasi bulunamadi'))

    print("\n" + "-" * 70)
    print("TEST 6: SHA-256 butunluk kontrolu")
    print("-" * 70)

    decrypted_sha256 = hashlib.sha256(decrypted_data).hexdigest()
    original_sha256 = hashlib.sha256(original_binary).hexdigest() if os.path.exists(original_weights_bin) else 'N/A'

    print(f"  Cozumlenen SHA-256 : {decrypted_sha256}")
    print(f"  Orijinal SHA-256   : {original_sha256}")

    if decrypted_sha256 == original_sha256:
        print(f"  Hash eslesmesi     : EVET")
        print(f"  SONUC: BASARILI")
        test_results.append(('SHA-256 Dogrulama', True, 'Hash eslesti'))
    elif original_sha256 == 'N/A':
        print(f"  ATLANDI: Orijinal dosya yok")
        test_results.append(('SHA-256 Dogrulama', None, 'Orijinal dosya yok'))
    else:
        print(f"  Hash eslesmesi     : HAYIR")
        print(f"  SONUC: BASARISIZ")
        test_results.append(('SHA-256 Dogrulama', False, 'Hash eslesmedi'))

    print("\n" + "-" * 70)
    print("TEST 7: Yanlis anahtar ile cozumleme testi (negatif test)")
    print("-" * 70)

    wrong_key = bytes([0xFF] * 32)

    try:
        wrong_decrypted = decrypt_data(
            parsed['ciphertext'],
            parsed['nonce'],
            parsed['auth_tag'],
            wrong_key,
            mode=parsed['encryption_mode']
        )
        print(f"  SONUC: BASARISIZ - Yanlis anahtar ile cozumleme basarili olmamali!")
        test_results.append(('Yanlis Anahtar Testi', False, 'Yanlis anahtar kabul edildi'))
    except Exception as e:
        print(f"  Yanlis anahtar ile cozumleme beklendigi gibi basarisiz oldu.")
        print(f"  Hata mesaji: {type(e).__name__}")
        print(f"  SONUC: BASARILI (beklenen davranis)")
        test_results.append(('Yanlis Anahtar Testi', True, 'Yanlis anahtar reddedildi'))

    print("\n" + "-" * 70)
    print("TEST 8: Bozulmus veri ile cozumleme testi (tamper detection)")
    print("-" * 70)

    tampered_ciphertext = bytearray(parsed['ciphertext'])
    tampered_ciphertext[0] ^= 0xFF
    tampered_ciphertext = bytes(tampered_ciphertext)

    try:
        tampered_decrypted = decrypt_data(
            tampered_ciphertext,
            parsed['nonce'],
            parsed['auth_tag'],
            aes_key,
            mode=parsed['encryption_mode']
        )
        if parsed['encryption_mode'] == 'GCM':
            print(f"  SONUC: BASARISIZ - Bozulmus veri GCM'de tespit edilmeliydi!")
            test_results.append(('Tamper Detection', False, 'Bozulma tespit edilemedi'))
        else:
            print(f"  Not: CBC modunda tamper detection desteklenmez (beklenen).")
            test_results.append(('Tamper Detection', True, 'CBC modu - beklenen davranis'))
    except Exception as e:
        print(f"  Bozulmus veri beklendigi gibi tespit edildi.")
        print(f"  Hata mesaji: {type(e).__name__}")
        print(f"  SONUC: BASARILI (beklenen davranis)")
        test_results.append(('Tamper Detection', True, 'Bozulma tespit edildi'))

    print("\n" + "=" * 70)
    print("DOGRULAMA SONUC OZETI")
    print("=" * 70)

    passed = 0
    failed = 0
    skipped = 0

    for test_name, result, message in test_results:
        if result is True:
            status = "BASARILI"
            passed += 1
        elif result is False:
            status = "BASARISIZ"
            failed += 1
        else:
            status = "ATLANDI"
            skipped += 1
        print(f"  {test_name:30s} : {status:10s} | {message}")

    print("-" * 70)
    print(f"  Toplam: {len(test_results)} test | Basarili: {passed} | Basarisiz: {failed} | Atlanan: {skipped}")

    if failed == 0:
        print("\n  GENEL SONUC: TUM TESTLER BASARILI")
    else:
        print(f"\n  GENEL SONUC: {failed} TEST BASARISIZ OLDU")

    print("=" * 70)
    print("FAZ 1 TAMAMLANDI: Egitim -> Disa Aktarma -> Sifreleme -> Dogrulama")
    print("Sonraki faz: Faz 2 - FPGA uzerinde RO-PUF ve AES-256 donanim modulleri")
    print("=" * 70)

    verification_report = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'phase': 'Faz 1 - End-to-End Verification',
        'tests': [
            {
                'name': name,
                'passed': result,
                'message': msg
            }
            for name, result, msg in test_results
        ],
        'summary': {
            'total': len(test_results),
            'passed': passed,
            'failed': failed,
            'skipped': skipped
        }
    }

    report_path = os.path.join(encrypt_dir, 'verification_report.json')
    with open(report_path, 'w') as f:
        json.dump(verification_report, f, indent=2)
    print(f"\nDogrulama raporu kaydedildi: {report_path}")

    return failed == 0


if __name__ == '__main__':
    success = verify_encryption()
    sys.exit(0 if success else 1)
