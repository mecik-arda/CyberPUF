import os
import sys
import json
import struct
import hashlib
import secrets
import datetime
import numpy as np
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad


CYPHERPUF_STATIC_AES_KEY = bytes([
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
    0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A
])

ENCRYPTED_FILE_MAGIC = b'CPFE'
ENCRYPTED_VERSION_MAJOR = 1
ENCRYPTED_VERSION_MINOR = 0


def derive_key_from_puf_simulation(raw_puf_key):
    key_hash = hashlib.sha256(raw_puf_key).digest()
    return key_hash


def encrypt_aes256_gcm(plaintext_data, aes_key):
    nonce = secrets.token_bytes(12)

    cipher = AES.new(aes_key, AES.MODE_GCM, nonce=nonce)

    ciphertext, auth_tag = cipher.encrypt_and_digest(plaintext_data)

    return ciphertext, nonce, auth_tag


def encrypt_aes256_cbc(plaintext_data, aes_key):
    iv = secrets.token_bytes(16)

    cipher = AES.new(aes_key, AES.MODE_CBC, iv=iv)

    padded_data = pad(plaintext_data, AES.block_size)

    ciphertext = cipher.encrypt(padded_data)

    return ciphertext, iv


def build_encrypted_binary(ciphertext, nonce, auth_tag, metadata, mode='GCM'):
    output = bytearray()

    output.extend(ENCRYPTED_FILE_MAGIC)

    output.extend(struct.pack('<B', ENCRYPTED_VERSION_MAJOR))
    output.extend(struct.pack('<B', ENCRYPTED_VERSION_MINOR))

    if mode == 'GCM':
        output.extend(struct.pack('<B', 0x01))
    elif mode == 'CBC':
        output.extend(struct.pack('<B', 0x02))

    output.extend(struct.pack('<B', 0x00))

    metadata_json = json.dumps(metadata).encode('utf-8')
    output.extend(struct.pack('<I', len(metadata_json)))
    output.extend(metadata_json)

    if mode == 'GCM':
        output.extend(struct.pack('<B', len(nonce)))
        output.extend(nonce)
        output.extend(struct.pack('<B', len(auth_tag)))
        output.extend(auth_tag)
    elif mode == 'CBC':
        output.extend(struct.pack('<B', len(nonce)))
        output.extend(nonce)

    output.extend(struct.pack('<Q', len(ciphertext)))
    output.extend(ciphertext)

    return bytes(output)


def generate_c_header(encrypted_data, output_path, array_name='encrypted_weights'):
    lines = []

    lines.append(f'#ifndef CYPHERPUF_ENCRYPTED_WEIGHTS_H')
    lines.append(f'#define CYPHERPUF_ENCRYPTED_WEIGHTS_H')
    lines.append(f'')
    lines.append(f'#include <stdint.h>')
    lines.append(f'')
    lines.append(f'#define ENCRYPTED_DATA_SIZE {len(encrypted_data)}')
    lines.append(f'')
    lines.append(f'static const uint8_t {array_name}[ENCRYPTED_DATA_SIZE] = {{')

    bytes_per_line = 16
    for i in range(0, len(encrypted_data), bytes_per_line):
        chunk = encrypted_data[i:i + bytes_per_line]
        hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
        if i + bytes_per_line < len(encrypted_data):
            lines.append(f'    {hex_values},')
        else:
            lines.append(f'    {hex_values}')

    lines.append(f'}};')
    lines.append(f'')
    lines.append(f'#endif')
    lines.append(f'')

    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

    return output_path


def generate_c_header_chunked(encrypted_data, output_dir, array_name='encrypted_weights', chunk_size=65536):
    os.makedirs(output_dir, exist_ok=True)

    num_chunks = (len(encrypted_data) + chunk_size - 1) // chunk_size
    chunk_files = []

    for chunk_idx in range(num_chunks):
        start = chunk_idx * chunk_size
        end = min(start + chunk_size, len(encrypted_data))
        chunk_data = encrypted_data[start:end]

        chunk_filename = f'{array_name}_chunk_{chunk_idx:04d}.h'
        chunk_path = os.path.join(output_dir, chunk_filename)

        lines = []
        guard = f'CYPHERPUF_{array_name.upper()}_CHUNK_{chunk_idx:04d}_H'
        lines.append(f'#ifndef {guard}')
        lines.append(f'#define {guard}')
        lines.append(f'')
        lines.append(f'#include <stdint.h>')
        lines.append(f'')
        lines.append(f'#define CHUNK_{chunk_idx:04d}_SIZE {len(chunk_data)}')
        lines.append(f'#define CHUNK_{chunk_idx:04d}_OFFSET {start}')
        lines.append(f'')
        lines.append(f'static const uint8_t {array_name}_chunk_{chunk_idx:04d}[CHUNK_{chunk_idx:04d}_SIZE] = {{')

        bytes_per_line = 16
        for i in range(0, len(chunk_data), bytes_per_line):
            sub_chunk = chunk_data[i:i + bytes_per_line]
            hex_values = ', '.join(f'0x{b:02X}' for b in sub_chunk)
            if i + bytes_per_line < len(chunk_data):
                lines.append(f'    {hex_values},')
            else:
                lines.append(f'    {hex_values}')

        lines.append(f'}};')
        lines.append(f'')
        lines.append(f'#endif')
        lines.append(f'')

        with open(chunk_path, 'w') as f:
            f.write('\n'.join(lines))

        chunk_files.append(chunk_path)

    master_header_path = os.path.join(output_dir, f'{array_name}_master.h')
    master_lines = []
    master_guard = f'CYPHERPUF_{array_name.upper()}_MASTER_H'
    master_lines.append(f'#ifndef {master_guard}')
    master_lines.append(f'#define {master_guard}')
    master_lines.append(f'')
    master_lines.append(f'#include <stdint.h>')
    master_lines.append(f'')
    master_lines.append(f'#define TOTAL_ENCRYPTED_SIZE {len(encrypted_data)}')
    master_lines.append(f'#define TOTAL_CHUNKS {num_chunks}')
    master_lines.append(f'#define CHUNK_SIZE {chunk_size}')
    master_lines.append(f'')

    for chunk_idx in range(num_chunks):
        chunk_filename = f'{array_name}_chunk_{chunk_idx:04d}.h'
        master_lines.append(f'#include "{chunk_filename}"')

    master_lines.append(f'')

    master_lines.append(f'static const uint8_t* {array_name}_chunks[TOTAL_CHUNKS] = {{')
    for chunk_idx in range(num_chunks):
        if chunk_idx < num_chunks - 1:
            master_lines.append(f'    {array_name}_chunk_{chunk_idx:04d},')
        else:
            master_lines.append(f'    {array_name}_chunk_{chunk_idx:04d}')
    master_lines.append(f'}};')
    master_lines.append(f'')

    master_lines.append(f'static const uint32_t {array_name}_chunk_sizes[TOTAL_CHUNKS] = {{')
    for chunk_idx in range(num_chunks):
        start = chunk_idx * chunk_size
        end = min(start + chunk_size, len(encrypted_data))
        size = end - start
        if chunk_idx < num_chunks - 1:
            master_lines.append(f'    {size},')
        else:
            master_lines.append(f'    {size}')
    master_lines.append(f'}};')
    master_lines.append(f'')

    master_lines.append(f'#endif')
    master_lines.append(f'')

    with open(master_header_path, 'w') as f:
        f.write('\n'.join(master_lines))

    chunk_files.append(master_header_path)
    return chunk_files


def encrypt_weights(weight_binary_path=None, encryption_mode='GCM'):
    base_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'output')
    export_dir = os.path.join(base_dir, 'exported_weights')
    encrypt_dir = os.path.join(base_dir, 'encrypted_weights')
    c_header_dir = os.path.join(encrypt_dir, 'c_headers')
    os.makedirs(encrypt_dir, exist_ok=True)
    os.makedirs(c_header_dir, exist_ok=True)

    if weight_binary_path is None:
        weight_binary_path = os.path.join(export_dir, 'cypherpuf_weights.bin')

    if not os.path.exists(weight_binary_path):
        print(f"HATA: Agirlik dosyasi bulunamadi: {weight_binary_path}")
        print("Lutfen once export_weights.py betigini calistirin.")
        sys.exit(1)

    print("=" * 70)
    print("CypherPUF - Faz 1: AES-256 Agirlik Sifreleme")
    print("Gelistirici: Arda Mecik")
    print(f"Sifreleme Modu: AES-256-{encryption_mode}")
    print("=" * 70)

    print("\n[1/7] Duz metin (plaintext) agirlik dosyasi okunuyor...")
    with open(weight_binary_path, 'rb') as f:
        plaintext_data = f.read()
    print(f"  Dosya boyutu  : {len(plaintext_data):,} byte ({len(plaintext_data) / (1024 * 1024):.2f} MB)")

    plaintext_sha256 = hashlib.sha256(plaintext_data).hexdigest()
    print(f"  SHA-256 ozeti : {plaintext_sha256}")

    print("\n[2/7] PUF simule edilen AES-256 anahtari hazirlaniyor...")
    aes_key = derive_key_from_puf_simulation(CYPHERPUF_STATIC_AES_KEY)
    print(f"  Anahtar uzunlugu : {len(aes_key) * 8} bit")
    print(f"  Anahtar (hex)    : {aes_key.hex()}")
    print(f"  Not: Bu statik anahtar, Faz 2'de FPGA uzerindeki RO-PUF tarafindan")
    print(f"       uretilen donanim-ozgu anahtarla degistirilecektir.")

    key_info_path = os.path.join(encrypt_dir, 'puf_simulated_key.json')
    key_info = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'description': 'PUF simulated AES-256 key (to be replaced by hardware PUF in Phase 2)',
        'raw_key_hex': CYPHERPUF_STATIC_AES_KEY.hex(),
        'derived_key_hex': aes_key.hex(),
        'key_length_bits': len(aes_key) * 8,
        'derivation_method': 'SHA-256',
        'timestamp': datetime.datetime.now().isoformat()
    }
    with open(key_info_path, 'w') as f:
        json.dump(key_info, f, indent=2)
    print(f"  Anahtar bilgisi kaydedildi: {key_info_path}")

    print(f"\n[3/7] AES-256-{encryption_mode} ile sifreleme gerceklestiriliyor...")

    if encryption_mode == 'GCM':
        ciphertext, nonce, auth_tag = encrypt_aes256_gcm(plaintext_data, aes_key)
        print(f"  Sifreli veri boyutu  : {len(ciphertext):,} byte")
        print(f"  Nonce (12 byte, hex) : {nonce.hex()}")
        print(f"  Auth Tag (16 byte)   : {auth_tag.hex()}")
    elif encryption_mode == 'CBC':
        ciphertext, nonce = encrypt_aes256_cbc(plaintext_data, aes_key)
        auth_tag = b''
        print(f"  Sifreli veri boyutu  : {len(ciphertext):,} byte")
        print(f"  IV (16 byte, hex)    : {nonce.hex()}")
        print(f"  Not: CBC modunda padding eklendi ({len(ciphertext) - len(plaintext_data)} byte)")

    ciphertext_sha256 = hashlib.sha256(ciphertext).hexdigest()
    print(f"  Sifreleme SHA-256    : {ciphertext_sha256}")

    print("\n[4/7] Dogrulama: Sifreli veriyi cozumleme (decrypt) testi...")

    if encryption_mode == 'GCM':
        verify_cipher = AES.new(aes_key, AES.MODE_GCM, nonce=nonce)
        decrypted_data = verify_cipher.decrypt_and_verify(ciphertext, auth_tag)
    elif encryption_mode == 'CBC':
        from Crypto.Util.Padding import unpad
        verify_cipher = AES.new(aes_key, AES.MODE_CBC, iv=nonce)
        decrypted_padded = verify_cipher.decrypt(ciphertext)
        decrypted_data = unpad(decrypted_padded, AES.block_size)

    if decrypted_data == plaintext_data:
        print(f"  BASARILI: Cozumlenen veri orijinal veriyle ESLESIR.")
        print(f"  Orijinal boyut   : {len(plaintext_data):,} byte")
        print(f"  Cozumlenen boyut : {len(decrypted_data):,} byte")
    else:
        print(f"  HATA: Cozumlenen veri orijinal veriyle ESLESMEZ!")
        sys.exit(1)

    print("\n[5/7] Sifreli ikili (binary) dosya olusturuluyor...")

    encryption_metadata = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'encryption_mode': f'AES-256-{encryption_mode}',
        'plaintext_size': len(plaintext_data),
        'ciphertext_size': len(ciphertext),
        'plaintext_sha256': plaintext_sha256,
        'ciphertext_sha256': ciphertext_sha256,
        'timestamp': datetime.datetime.now().isoformat(),
        'key_source': 'PUF_SIMULATED_STATIC'
    }

    encrypted_binary = build_encrypted_binary(
        ciphertext, nonce, auth_tag, encryption_metadata, mode=encryption_mode
    )

    encrypted_bin_path = os.path.join(encrypt_dir, 'cypherpuf_encrypted_weights.bin')
    with open(encrypted_bin_path, 'wb') as f:
        f.write(encrypted_binary)
    print(f"  Sifreli dosya boyutu : {len(encrypted_binary):,} byte ({len(encrypted_binary) / (1024 * 1024):.2f} MB)")
    print(f"  Dosya kaydedildi     : {encrypted_bin_path}")

    raw_encrypted_path = os.path.join(encrypt_dir, 'cypherpuf_ciphertext_raw.bin')
    with open(raw_encrypted_path, 'wb') as f:
        f.write(ciphertext)
    print(f"  Ham sifreli veri     : {raw_encrypted_path}")

    nonce_path = os.path.join(encrypt_dir, 'cypherpuf_nonce.bin')
    with open(nonce_path, 'wb') as f:
        f.write(nonce)
    print(f"  Nonce/IV dosyasi     : {nonce_path}")

    if encryption_mode == 'GCM':
        tag_path = os.path.join(encrypt_dir, 'cypherpuf_auth_tag.bin')
        with open(tag_path, 'wb') as f:
            f.write(auth_tag)
        print(f"  Auth Tag dosyasi     : {tag_path}")

    print("\n[6/7] C header dosyalari olusturuluyor (donanima yukleme icin)...")

    single_header_path = os.path.join(c_header_dir, 'cypherpuf_encrypted_weights.h')
    if len(encrypted_binary) <= 1024 * 1024:
        generate_c_header(encrypted_binary, single_header_path)
        print(f"  Tekil header dosyasi : {single_header_path}")
    else:
        print(f"  Veri boyutu 1MB'den buyuk, parcali header olusturuluyor...")

    chunked_dir = os.path.join(c_header_dir, 'chunked')
    chunk_files = generate_c_header_chunked(
        encrypted_binary, chunked_dir,
        array_name='encrypted_weights',
        chunk_size=65536
    )
    print(f"  Parcali header dosyalari ({len(chunk_files)} adet):")
    for cf in chunk_files:
        print(f"    -> {cf}")

    nonce_header_path = os.path.join(c_header_dir, 'cypherpuf_nonce.h')
    nonce_header_lines = []
    nonce_header_lines.append('#ifndef CYPHERPUF_NONCE_H')
    nonce_header_lines.append('#define CYPHERPUF_NONCE_H')
    nonce_header_lines.append('')
    nonce_header_lines.append('#include <stdint.h>')
    nonce_header_lines.append('')
    nonce_header_lines.append(f'#define NONCE_SIZE {len(nonce)}')
    nonce_header_lines.append('')
    hex_values = ', '.join(f'0x{b:02X}' for b in nonce)
    nonce_header_lines.append(f'static const uint8_t aes_nonce[NONCE_SIZE] = {{ {hex_values} }};')
    nonce_header_lines.append('')

    if encryption_mode == 'GCM' and auth_tag:
        nonce_header_lines.append(f'#define AUTH_TAG_SIZE {len(auth_tag)}')
        nonce_header_lines.append('')
        hex_values_tag = ', '.join(f'0x{b:02X}' for b in auth_tag)
        nonce_header_lines.append(f'static const uint8_t aes_auth_tag[AUTH_TAG_SIZE] = {{ {hex_values_tag} }};')
        nonce_header_lines.append('')

    nonce_header_lines.append('#endif')
    nonce_header_lines.append('')

    with open(nonce_header_path, 'w') as f:
        f.write('\n'.join(nonce_header_lines))
    print(f"  Nonce header dosyasi : {nonce_header_path}")

    print("\n[7/7] Sifreleme ozet raporu olusturuluyor...")

    encryption_report = {
        'project': 'CypherPUF',
        'developer': 'Arda Mecik',
        'phase': 'Faz 1 - AES-256 Encryption',
        'encryption': {
            'algorithm': 'AES-256',
            'mode': encryption_mode,
            'key_length_bits': 256,
            'key_source': 'PUF Simulated (Static)',
            'nonce_length_bytes': len(nonce),
            'auth_tag_length_bytes': len(auth_tag) if auth_tag else 0
        },
        'data': {
            'plaintext_size_bytes': len(plaintext_data),
            'ciphertext_size_bytes': len(ciphertext),
            'encrypted_file_size_bytes': len(encrypted_binary),
            'overhead_bytes': len(encrypted_binary) - len(plaintext_data),
            'overhead_percentage': ((len(encrypted_binary) - len(plaintext_data)) / len(plaintext_data)) * 100,
            'plaintext_sha256': plaintext_sha256,
            'ciphertext_sha256': ciphertext_sha256
        },
        'files': {
            'encrypted_binary': encrypted_bin_path,
            'raw_ciphertext': raw_encrypted_path,
            'nonce_file': nonce_path,
            'c_header_single': single_header_path if len(encrypted_binary) <= 1024 * 1024 else 'N/A (too large)',
            'c_header_chunked_dir': chunked_dir,
            'c_header_nonce': nonce_header_path,
            'key_info': key_info_path
        },
        'verification': {
            'decrypt_test': 'PASSED',
            'data_integrity': 'VERIFIED'
        },
        'timestamp': datetime.datetime.now().isoformat()
    }

    report_path = os.path.join(encrypt_dir, 'encryption_report.json')
    with open(report_path, 'w') as f:
        json.dump(encryption_report, f, indent=2)
    print(f"  Rapor kaydedildi: {report_path}")

    print("\n" + "=" * 70)
    print("SIFRELEME OZETI")
    print("=" * 70)
    print(f"  Algoritma          : AES-256-{encryption_mode}")
    print(f"  Duz metin boyutu   : {len(plaintext_data):,} byte")
    print(f"  Sifreli boyut      : {len(encrypted_binary):,} byte")
    print(f"  Ek yuk (overhead)  : {len(encrypted_binary) - len(plaintext_data):,} byte ({encryption_report['data']['overhead_percentage']:.2f}%)")
    print(f"  Dogrulama          : BASARILI")
    print("=" * 70)
    print("FAZ 1 - ADIM 3 TAMAMLANDI: Agirliklar sifrelendi.")
    print("Sonraki adim: verify_encryption.py ile uctan uca dogrulama yapin.")
    print("=" * 70)

    return encrypted_binary, aes_key, nonce, auth_tag


if __name__ == '__main__':
    custom_weight_path = None
    custom_mode = 'GCM'

    if len(sys.argv) > 1:
        custom_weight_path = sys.argv[1]
    if len(sys.argv) > 2:
        custom_mode = sys.argv[2].upper()

    if custom_mode not in ('GCM', 'CBC'):
        print(f"HATA: Gecersiz sifreleme modu: {custom_mode}")
        print("Desteklenen modlar: GCM, CBC")
        sys.exit(1)

    encrypt_weights(
        weight_binary_path=custom_weight_path,
        encryption_mode=custom_mode
    )
