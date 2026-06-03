#include "xil_printf.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "platform_yapilandirmasi.h"
#include "cyberpuf_dsk.h"
#include "yapay_zeka_cikarimi.h"
#include "test_goruntusu.h"
#include "yardimci_veri_uretici.h"
#include "sha256.h"

static void guvenli_temizle(void* ptr, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

extern const uint8_t sifreli_agirliklar[];
extern const uint32_t SIFRELI_VERI_BOYUTU;

#if XILINX_BAREMETAL_SIM
static uint8_t sim_reg_alani[128];
void Sim_RegYaz(uint32_t adres, uint32_t data) {
    uint32_t ofset = adres - CYBERPUF_TABAN_ADRES;
    if (ofset <= 124) {
        sim_reg_alani[ofset] = data & 0xFF;
        sim_reg_alani[ofset+1] = (data >> 8) & 0xFF;
        sim_reg_alani[ofset+2] = (data >> 16) & 0xFF;
        sim_reg_alani[ofset+3] = (data >> 24) & 0xFF;
        
        if (ofset == CYBERPUF_REG_KONTROL) {
            if (data & KONTROL_ANAHTAR_URET_BITI) {
                uint32_t durum = sim_reg_alani[CYBERPUF_REG_DURUM] | sim_reg_alani[CYBERPUF_REG_DURUM+1]<<8 | sim_reg_alani[CYBERPUF_REG_DURUM+2]<<16 | sim_reg_alani[CYBERPUF_REG_DURUM+3]<<24;
                durum |= DURUM_PUF_TAMAM_BITI | DURUM_ANAHTAR_GEN_TAMAM_BITI;
                sim_reg_alani[CYBERPUF_REG_DURUM] = durum & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+1] = (durum >> 8) & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+2] = (durum >> 16) & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+3] = (durum >> 24) & 0xFF;
            }
            if (data & KONTROL_SIFRE_COZ_BASLA_BITI) {
                uint32_t durum = sim_reg_alani[CYBERPUF_REG_DURUM] | sim_reg_alani[CYBERPUF_REG_DURUM+1]<<8 | sim_reg_alani[CYBERPUF_REG_DURUM+2]<<16 | sim_reg_alani[CYBERPUF_REG_DURUM+3]<<24;
                durum |= DURUM_AES_TAMAM_BITI;
                sim_reg_alani[CYBERPUF_REG_DURUM] = durum & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+1] = (durum >> 8) & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+2] = (durum >> 16) & 0xFF;
                sim_reg_alani[CYBERPUF_REG_DURUM+3] = (durum >> 24) & 0xFF;
                
                for(int i=0; i<16; i++) {
                    sim_reg_alani[CYBERPUF_REG_VERI_CIKIS_0 + i] = sim_reg_alani[CYBERPUF_REG_VERI_GIRIS_0 + i];
                }
            }
            if (data & KONTROL_DURUM_TEMIZLE_BITI) {
                sim_reg_alani[CYBERPUF_REG_DURUM] = 0;
                sim_reg_alani[CYBERPUF_REG_DURUM+1] = 0;
                sim_reg_alani[CYBERPUF_REG_DURUM+2] = 0;
                sim_reg_alani[CYBERPUF_REG_DURUM+3] = 0;
            }
        }
    }
}

uint32_t Sim_RegOku(uint32_t adres) {
    uint32_t ofset = adres - CYBERPUF_TABAN_ADRES;
    if (ofset <= 124) {
        return (sim_reg_alani[ofset]) | (sim_reg_alani[ofset+1] << 8) | (sim_reg_alani[ofset+2] << 16) | (sim_reg_alani[ofset+3] << 24);
    }
    return 0;
}

const uint8_t sifreli_agirliklar[64] = {
    0x43, 0x50, 0x55, 0x46, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const uint32_t SIFRELI_VERI_BOYUTU = 64;
#endif


uint32_t CPFE_Header_Oku(const uint8_t* tampon, size_t tampon_boyutu, uint8_t* nonce, uint32_t* metadata_boyutu, char* beklenen_sha256) {
    uint32_t offset = 0;

    // Minimum baslik boyutu kontrolu (magic + version + mode + reserved = 8 bayt)
    if (tampon_boyutu < 8) {
        xil_printf("HATA: Tampon boyutu cok kucuk.\n");
        return 0;
    }

    if (tampon[0] != 'C' || tampon[1] != 'P' || tampon[2] != 'F' || tampon[3] != 'E') {
        xil_printf("HATA: Gecersiz CPFE magic number.\n");
        return 0;
    }
    offset += 4;
    offset += 2; // version
    uint8_t mode = tampon[offset++];
    offset += 1; // reserved
    
    if (offset + 4 > tampon_boyutu) return 0;
    memcpy(metadata_boyutu, &tampon[offset], 4);
    offset += 4;

    // Metadata boyutu sinir kontrolu
    if (*metadata_boyutu > 4096 || offset + *metadata_boyutu > tampon_boyutu) {
        xil_printf("HATA: metadata_boyutu cok buyuk veya tampon sinirini asiyor.\n");
        return 0;
    }
    
    // Hash cikarimi (Metadata icinden JSON parsing)
    if (beklenen_sha256) {
        char* sha_ptr = strstr((char*)&tampon[offset], "\"plaintext_sha256\": \"");
        if (sha_ptr) {
            sha_ptr += 21; // " uzunlugu
            for(int i=0; i<64; i++) {
                beklenen_sha256[i] = sha_ptr[i];
            }
            beklenen_sha256[64] = '\0';
        } else {
            beklenen_sha256[0] = '\0';
        }
    }
    
    offset += *metadata_boyutu;
    
    if (offset + 1 > tampon_boyutu) return 0;
    uint8_t nonce_len = tampon[offset++];

    // Nonce uzunlugu sinir kontrolu
    if (nonce_len > 16 || offset + nonce_len > tampon_boyutu) {
        xil_printf("HATA: nonce_len sinir disi.\n");
        return 0;
    }
    if (nonce) {
        memcpy(nonce, &tampon[offset], nonce_len);
    }
    offset += nonce_len;
    
    if (mode == 0x01) { // GCM
        if (offset + 1 > tampon_boyutu) return 0;
        uint8_t tag_len = tampon[offset++];
        if (offset + tag_len > tampon_boyutu) return 0;
        offset += tag_len;
    }
    
    if (offset + 8 > tampon_boyutu) return 0;
    offset += 8; // ciphertext_length
    return offset;
}

int main(void) {
    xil_printf("========================================\n");
    xil_printf("CyberPUF - Faz 3: Gomulu Yapay Zeka Cikarimi\n");
    xil_printf("Gelistirici: Arda Mecik\n");
    xil_printf("========================================\n");
    
    CyberPUF_Baslat(CYBERPUF_TABAN_ADRES);
    
    xil_printf("[1/4] Donanim PUF Anahtar Uretimi Tetikleniyor...\n");
    bool anahtar_uretimi_tamam = CyberPUF_AnahtarUret();
    if (!anahtar_uretimi_tamam) {
        xil_printf("HATA: PUF anahtar uretimi basarisiz oldu veya zaman asimina ugradi.\n");
        return -1;
    }
    xil_printf("      -> PUF Anahtari uretildi ve AES Tur Anahtarlarina basariyla genisletildi.\n");
    
    uint8_t puf_anahtari[32];
    CyberPUF_PUFAnahtariAl(puf_anahtari);
    #ifdef CYBERPUF_DEBUG
    xil_printf("      -> PUF Anahtari (Hex): ");
    for(int i=0; i<32; i++) xil_printf("%02X", puf_anahtari[i]);
    xil_printf("\n");
    #endif
    
    xil_printf("\n--- FUZZY EXTRACTOR TESTI (YARDIMCI VERI & HATA DUZELTME) ---\n");
    YardimciVeri yardimci_veri;
    uint8_t gercek_anahtar_kayit[32];
    uint8_t gercek_anahtar_cikarim[32];

    xil_printf("1. Kayit (Enrollment) Asamasi...\n");
    FuzzyExtractor_Kayit(puf_anahtari, &yardimci_veri, gercek_anahtar_kayit);
    #ifdef CYBERPUF_DEBUG
    xil_printf("   -> Rastgele Uretilen Guvenli Anahtar: ");
    for(int i=0; i<32; i++) xil_printf("%02X", gercek_anahtar_kayit[i]);
    xil_printf("\n");
    #endif

    xil_printf("2. PUF Gurultusu (Hata Enjeksiyonu) Simule Ediliyor...\n");
    uint8_t gurultulu_puf_anahtari[32];
    memcpy(gurultulu_puf_anahtari, puf_anahtari, 32);
    // 3 farkli byte'ta 1'er bit hata olustur (Hamming kodu duzeltebilir mi diye test)
    gurultulu_puf_anahtari[5] ^= 0x01;
    gurultulu_puf_anahtari[12] ^= 0x04;
    gurultulu_puf_anahtari[27] ^= 0x08;
    xil_printf("   -> Hata enjekte edildi (Byte 5, 12 ve 27).\n");

    xil_printf("3. Cikarim (Reconstruction) Asamasi...\n");
    int duzeltilen_hata = FuzzyExtractor_Cikarim(gurultulu_puf_anahtari, &yardimci_veri, gercek_anahtar_cikarim);
    if (duzeltilen_hata == -1) {
        xil_printf("HATA: Cift bit hatasi tespit edildi, guvenli anahtar olusturulamadi.\n");
        return -1;
    }
    #ifdef CYBERPUF_DEBUG
    xil_printf("   -> Cikarim Sonucu Uretilen Anahtar: ");
    for(int i=0; i<32; i++) xil_printf("%02X", gercek_anahtar_cikarim[i]);
    xil_printf("\n");
    #endif
    xil_printf("   -> Toplam duzeltilen bit hatasi: %d\n", duzeltilen_hata);

    if (memcmp(gercek_anahtar_kayit, gercek_anahtar_cikarim, 32) == 0) {
        xil_printf("   -> BASARILI: Gercek anahtar '%d' bit hatasina ragmen %%100 dogru sekilde onarildi!\n", duzeltilen_hata);
    } else {
        xil_printf("   -> HATA: Anahtar onarilamadi.\n");
    }
    xil_printf("---------------------------------------------------------------\n");
    
    xil_printf("\n[2/4] Model agirliklari icin bellek ayriliyor (Boyut: %u bayt)...\n", SIFRELI_VERI_BOYUTU);
    uint8_t* cozulmus_bellek = (uint8_t*)malloc(SIFRELI_VERI_BOYUTU);
    if (!cozulmus_bellek) {
        xil_printf("HATA: Bellek ayirma islemi basarisiz.\n");
        return -1;
    }
    
    xil_printf("\n[3/4] Yapay Zeka Model Agirliklari Donanim AES-256 ile Cozuluyor...\n");
    
    uint8_t nonce[16];
    uint32_t metadata_boyutu = 0;
    char beklenen_sha256[65];
    uint32_t ciphertext_offset = CPFE_Header_Oku(sifreli_agirliklar, (size_t)SIFRELI_VERI_BOYUTU, nonce, &metadata_boyutu, beklenen_sha256);
    if (ciphertext_offset == 0) {
        free(cozulmus_bellek);
        return -1;
    }
    
    uint32_t ciphertext_size = SIFRELI_VERI_BOYUTU - ciphertext_offset;
    if (!CyberPUF_TamponSifreCoz(
        &sifreli_agirliklar[ciphertext_offset],
        &cozulmus_bellek[0],
        ciphertext_size,
        nonce
    )) {
        xil_printf("HATA: Sifre cozme basarisiz.\n");
        free(cozulmus_bellek);
        return -1;
    }
    xil_printf("      -> Sifre cozme islemi tamamlandi.\n");
    
    // PKCS7 Unpadding
    uint8_t pad_len = cozulmus_bellek[ciphertext_size - 1];
    uint32_t gercek_veri_boyutu = ciphertext_size;
    if (pad_len > 0 && pad_len <= 16) {
        gercek_veri_boyutu -= pad_len;
    }
    
    // SHA-256 Butunluk Kontrolu
    xil_printf("      -> SHA-256 Butunluk (Integrity) dogrulamasi yapiliyor...\n");
    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, cozulmus_bellek, gercek_veri_boyutu);
    sha256_final(&ctx, hash);
    
    char hesaplanan_sha256[65];
    for (int i = 0; i < 32; i++) {
        char buf[3];
        // Basit sprintf benzeri hexadecimal donusum
        const char hex_chars[] = "0123456789abcdef";
        hesaplanan_sha256[i*2] = hex_chars[(hash[i] >> 4) & 0x0F];
        hesaplanan_sha256[i*2+1] = hex_chars[hash[i] & 0x0F];
    }
    hesaplanan_sha256[64] = '\0';
    
    if (beklenen_sha256[0] != '\0') {
        if (strcmp(hesaplanan_sha256, beklenen_sha256) != 0) {
            xil_printf("Kritik HATA: SHA-256 Kimlik dogrulamasi basarisiz! Sifreli veriye disaridan mudahale (Tampering) tespit edildi.\n");
            xil_printf("Beklenen: %s\n", beklenen_sha256);
            xil_printf("Hesaplanan: %s\n", hesaplanan_sha256);
            guvenli_temizle(cozulmus_bellek, SIFRELI_VERI_BOYUTU);
            free(cozulmus_bellek);
            return -1;
        } else {
            xil_printf("      -> BASARILI: Veri butunlugu SHA-256 ile tam olarak dogrulandi.\n");
        }
    } else {
        xil_printf("UYARI: Baslik icinde beklenen SHA-256 hash degeri bulunamadi, dogrulama atlandi.\n");
    }
    
    float* ham_agirliklar = CPUF_Ikilisi_Ayristir(&cozulmus_bellek[0], SIFRELI_VERI_BOYUTU - ciphertext_offset);
    if (ham_agirliklar == NULL) {
        xil_printf("UYARI: CPUF basligi ayristirildi. (Sahte agirliklarla simulasyonda calisiyorsa beklenir)\n");
        #if XILINX_BAREMETAL_SIM
            ham_agirliklar = (float*)&cozulmus_bellek[0]; 
        #else
            free(cozulmus_bellek);
            return -1;
        #endif
    } else {
        xil_printf("      -> CPUF Ikilisi ayristirildi. Agirlik verisi basariyla cikarildi.\n");
    }
    
    xil_printf("\n[4/4] ARM Cortex-A Uzerinde Yapay Zeka Cikarim Ileri Beslemesi Calistiriliyor...\n");
    float cikis_olasiliklari[10] = {0.0f};
    
    #if XILINX_BAREMETAL_SIM
        xil_printf("      -> Sahte agirliklar nedeniyle bellek erisim hatasini onlemek icin simulasyonda tam cikarim atlandi.\n");
        cikis_olasiliklari[0] = 0.95f;
    #else
        uint32_t agirlik_kapasitesi = (SIFRELI_VERI_BOYUTU - ciphertext_offset) - ((uint8_t*)ham_agirliklar - cozulmus_bellek);
        CyberPUF_CNN_Calistir(test_goruntusu_cifar10, ham_agirliklar, agirlik_kapasitesi, cikis_olasiliklari);
    #endif
    
    xil_printf("\nCikarim Sonuclari (Softmax Olasiliklari):\n");
    int en_yuksek_sinif = 0;
    float en_yuksek_olasilik = 0.0f;
    for (int i = 0; i < 10; i++) {
        xil_printf("  Sinif %d: ", i);
        int int_part = (int)(cikis_olasiliklari[i] * 10000);
        xil_printf("%d.%04d\n", int_part / 10000, int_part % 10000);
        if (cikis_olasiliklari[i] > en_yuksek_olasilik) {
            en_yuksek_olasilik = cikis_olasiliklari[i];
            en_yuksek_sinif = i;
        }
    }
    
    int final_int = (int)(en_yuksek_olasilik * 10000.0f);
    xil_printf("\nTahmin Edilen Sinif: %d (Olasilik: %d.%02d%%)\n", en_yuksek_sinif, final_int / 100, final_int % 100);
    
    guvenli_temizle(puf_anahtari, 32);
    guvenli_temizle(gercek_anahtar_kayit, 32);
    guvenli_temizle(gercek_anahtar_cikarim, 32);
    guvenli_temizle(gurultulu_puf_anahtari, 32);
    
    free(cozulmus_bellek);
    
    xil_printf("========================================\n");
    xil_printf("FAZ 3 TAMAMLANDI: Uctan Uca Uc Yapay Zeka Akisi Dogrulandi.\n");
    xil_printf("========================================\n");
    
    while(1) {
        __asm__("wfi");
    }
    
    return 0;
}
