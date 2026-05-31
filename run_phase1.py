import os
import sys
import time


def run_full_pipeline(epoch_sayisi=50, yigin_boyutu=128, ogrenme_orani=0.001, sifreleme_modu='GCM'):
    print("=" * 70)
    print("CypherPUF - Faz 1: Tam Pipeline Calistirma")
    print("Gelistirici: Arda Mecik")
    print("=" * 70)
    print(f"  Epoch sayisi       : {epoch_sayisi}")
    print(f"  Batch boyutu       : {yigin_boyutu}")
    print(f"  Ogrenme hizi       : {ogrenme_orani}")
    print(f"  Sifreleme modu     : AES-256-{sifreleme_modu}")
    print("=" * 70)

    toplam_baslangic = time.time()

    print("\n\n")
    print("#" * 70)
    print("# ADIM 1/4: CNN MODEL EGITIMI")
    print("#" * 70)

    adim1_baslangic = time.time()

    from ai_sifreleme.train_model import train_model
    model, gecmis = train_model(
        epochs=epoch_sayisi,
        batch_size=yigin_boyutu,
        learning_rate=ogrenme_orani
    )

    adim1_suresi = time.time() - adim1_baslangic
    print(f"\n  Adim 1 suresi: {adim1_suresi:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 2/4: AGIRLIK DISA AKTARMA")
    print("#" * 70)

    adim2_baslangic = time.time()

    from ai_sifreleme.export_weights import export_weights
    ikili_veri, agirlik_manifestosu, sha256_hash_degeri = export_weights()

    adim2_suresi = time.time() - adim2_baslangic
    print(f"\n  Adim 2 suresi: {adim2_suresi:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 3/4: AES-256 SIFRELEME")
    print("#" * 70)

    adim3_baslangic = time.time()

    from ai_sifreleme.encrypt_weights import encrypt_weights
    sifreli_ikili_veri, aes_anahtari, nonce_degeri, kimlik_dogrulama_etiketi = encrypt_weights(
        encryption_mode=sifreleme_modu
    )

    adim3_suresi = time.time() - adim3_baslangic
    print(f"\n  Adim 3 suresi: {adim3_suresi:.1f} saniye")

    print("\n\n")
    print("#" * 70)
    print("# ADIM 4/4: UCTAN UCA DOGRULAMA")
    print("#" * 70)

    adim4_baslangic = time.time()

    from ai_sifreleme.verify_encryption import verify_encryption
    dogrulama_basarili_mi = verify_encryption()

    adim4_suresi = time.time() - adim4_baslangic
    print(f"\n  Adim 4 suresi: {adim4_suresi:.1f} saniye")

    toplam_sure = time.time() - toplam_baslangic

    print("\n\n")
    print("=" * 70)
    print("PIPELINE TAMAMLANDI")
    print("=" * 70)
    print(f"  Adim 1 (Egitim)     : {adim1_suresi:8.1f} saniye")
    print(f"  Adim 2 (Disa Aktar) : {adim2_suresi:8.1f} saniye")
    print(f"  Adim 3 (Sifreleme)  : {adim3_suresi:8.1f} saniye")
    print(f"  Adim 4 (Dogrulama)  : {adim4_suresi:8.1f} saniye")
    print(f"  TOPLAM              : {toplam_sure:8.1f} saniye")
    print(f"  Dogrulama Sonucu    : {'BASARILI' if dogrulama_basarili_mi else 'BASARISIZ'}")
    print("=" * 70)

    return dogrulama_basarili_mi


if __name__ == '__main__':
    pipeline_epoch_sayisi = 50
    pipeline_yigin_boyutu = 128
    pipeline_ogrenme_orani = 0.001
    pipeline_modu = 'GCM'

    if len(sys.argv) > 1:
        pipeline_epoch_sayisi = int(sys.argv[1])
    if len(sys.argv) > 2:
        pipeline_yigin_boyutu = int(sys.argv[2])
    if len(sys.argv) > 3:
        pipeline_ogrenme_orani = float(sys.argv[3])
    if len(sys.argv) > 4:
        pipeline_modu = sys.argv[4].upper()

    basari_durumu = run_full_pipeline(
        epoch_sayisi=pipeline_epoch_sayisi,
        yigin_boyutu=pipeline_yigin_boyutu,
        ogrenme_orani=pipeline_ogrenme_orani,
        sifreleme_modu=pipeline_modu
    )

    sys.exit(0 if basari_durumu else 1)
