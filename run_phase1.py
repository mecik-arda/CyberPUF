import os
import sys
import time


def run_full_pipeline(epochs=50, batch_size=128, learning_rate=0.001, encryption_mode='GCM'):
    print("=" * 70)
    print("CypherPUF - Faz 1: Tam Pipeline Calistirma")
    print("Gelistirici: Arda Mecik")
    print("=" * 70)
    print(f"  Epoch sayisi       : {epochs}")
    print(f"  Batch boyutu       : {batch_size}")
    print(f"  Ogrenme hizi       : {learning_rate}")
    print(f"  Sifreleme modu     : AES-256-{encryption_mode}")
    print("=" * 70)

    total_start = time.time()

    print("\n\n")
    print("#" * 70)
    print("# ADIM 1/4: CNN MODEL EGITIMI")
    print("#" * 70)

    step1_start = time.time()

    from phase1_ai_encryption.train_model import train_model
    model, history = train_model(
        epochs=epochs,
        batch_size=batch_size,
        learning_rate=learning_rate
    )

    step1_time = time.time() - step1_start
    print(f"\n  Adim 1 suresi: {step1_time:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 2/4: AGIRLIK DISA AKTARMA")
    print("#" * 70)

    step2_start = time.time()

    from phase1_ai_encryption.export_weights import export_weights
    binary_data, weight_manifest, sha256_hash = export_weights()

    step2_time = time.time() - step2_start
    print(f"\n  Adim 2 suresi: {step2_time:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 3/4: AES-256 SIFRELEME")
    print("#" * 70)

    step3_start = time.time()

    from phase1_ai_encryption.encrypt_weights import encrypt_weights
    encrypted_binary, aes_key, nonce, auth_tag = encrypt_weights(
        encryption_mode=encryption_mode
    )

    step3_time = time.time() - step3_start
    print(f"\n  Adim 3 suresi: {step3_time:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 4/4: UCTAN UCA DOGRULAMA")
    print("#" * 70)

    step4_start = time.time()

    from phase1_ai_encryption.verify_encryption import verify_encryption
    verification_passed = verify_encryption()

    step4_time = time.time() - step4_start
    print(f"\n  Adim 4 suresi: {step4_time:.1f} saniye")

    total_time = time.time() - total_start

    print("\n\n")
    print("=" * 70)
    print("PIPELINE TAMAMLANDI")
    print("=" * 70)
    print(f"  Adim 1 (Egitim)     : {step1_time:8.1f} saniye")
    print(f"  Adim 2 (Disa Aktar) : {step2_time:8.1f} saniye")
    print(f"  Adim 3 (Sifreleme)  : {step3_time:8.1f} saniye")
    print(f"  Adim 4 (Dogrulama)  : {step4_time:8.1f} saniye")
    print(f"  TOPLAM              : {total_time:8.1f} saniye")
    print(f"  Dogrulama Sonucu    : {'BASARILI' if verification_passed else 'BASARISIZ'}")
    print("=" * 70)

    return verification_passed


if __name__ == '__main__':
    pipeline_epochs = 50
    pipeline_batch_size = 128
    pipeline_lr = 0.001
    pipeline_mode = 'GCM'

    if len(sys.argv) > 1:
        pipeline_epochs = int(sys.argv[1])
    if len(sys.argv) > 2:
        pipeline_batch_size = int(sys.argv[2])
    if len(sys.argv) > 3:
        pipeline_lr = float(sys.argv[3])
    if len(sys.argv) > 4:
        pipeline_mode = sys.argv[4].upper()

    success = run_full_pipeline(
        epochs=pipeline_epochs,
        batch_size=pipeline_batch_size,
        learning_rate=pipeline_lr,
        encryption_mode=pipeline_mode
    )

    sys.exit(0 if success else 1)
