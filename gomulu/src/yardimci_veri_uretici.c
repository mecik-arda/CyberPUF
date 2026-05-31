#include "yardimci_veri_uretici.h"
#include <stdlib.h>
#include <time.h>

// Hamming(7,4) Parite Hesaplama (3 bit dondurur)
static uint8_t Hamming74_PariteHesapla(uint8_t d) {
    uint8_t d1 = d & 1;
    uint8_t d2 = (d >> 1) & 1;
    uint8_t d3 = (d >> 2) & 1;
    uint8_t d4 = (d >> 3) & 1;

    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p3 = d2 ^ d3 ^ d4;

    return (p3 << 2) | (p2 << 1) | p1;
}

// Hamming(7,4) Hata Duzeltme
static uint8_t Hamming74_HataDuzelt(uint8_t gurultulu_d, uint8_t orijinal_p, int* hata_sayisi) {
    uint8_t yeni_p = Hamming74_PariteHesapla(gurultulu_d);
    uint8_t sendrom = yeni_p ^ orijinal_p;
    
    if (sendrom == 0) {
        return gurultulu_d; // Hata yok
    }
    
    if (hata_sayisi) {
        (*hata_sayisi)++;
    }

    if (sendrom == 3)      gurultulu_d ^= 0x1;
    else if (sendrom == 5) gurultulu_d ^= 0x2;
    else if (sendrom == 6) gurultulu_d ^= 0x4;
    else if (sendrom == 7) gurultulu_d ^= 0x8;
    // sendrom 1, 2, 4 ise parite bitlerinde hata olmustur, veri dogrudur.
    
    return gurultulu_d & 0x0F;
}

void FuzzyExtractor_Kayit(const uint8_t* puf_ham_anahtar, YardimciVeri* yardimci_veri, uint8_t* guvenli_anahtar) {
    // 1. Rastgele bir guvenli anahtar (AES anahtari) uret.
    // Gercek bir donanimda True Random Number Generator (TRNG) kullanilir.
    // Biz burada PUF ham anahtarindan elde edilen degeri seed (tohum) olarak kullaniyoruz.
    uint32_t seed_val = puf_ham_anahtar[0] | (puf_ham_anahtar[1] << 8) | (puf_ham_anahtar[2] << 16) | (puf_ham_anahtar[3] << 24);
    srand(seed_val);
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        guvenli_anahtar[i] = rand() & 0xFF;
    }

    // 2. Helper Data 1: Code-Offset XOR Maskesi
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        yardimci_veri->xor_maskesi[i] = puf_ham_anahtar[i] ^ guvenli_anahtar[i];
    }

    // 3. Helper Data 2: Hamming(7,4) Parite Verisi
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        uint8_t alt_nibble = guvenli_anahtar[i] & 0x0F;
        uint8_t ust_nibble = (guvenli_anahtar[i] >> 4) & 0x0F;

        uint8_t p_alt = Hamming74_PariteHesapla(alt_nibble);
        uint8_t p_ust = Hamming74_PariteHesapla(ust_nibble);

        // 6 biti tek bir byte icine yerlestir: [0 0 P_ust P_alt]
        yardimci_veri->parite_verisi[i] = (p_ust << 3) | p_alt;
    }
}

int FuzzyExtractor_Cikarim(const uint8_t* puf_yeni_anahtar, const YardimciVeri* yardimci_veri, uint8_t* guvenli_anahtar) {
    int toplam_hata = 0;

    // 1. Code-Offset ile gurultulu (aday) anahtari elde et
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        uint8_t gurultulu_bayt = puf_yeni_anahtar[i] ^ yardimci_veri->xor_maskesi[i];

        // 2. Hamming(7,4) paritesi ile varsa bit hatalarini duzelt
        uint8_t alt_nibble = gurultulu_bayt & 0x0F;
        uint8_t ust_nibble = (gurultulu_bayt >> 4) & 0x0F;

        uint8_t p_alt_orijinal = yardimci_veri->parite_verisi[i] & 0x07;
        uint8_t p_ust_orijinal = (yardimci_veri->parite_verisi[i] >> 3) & 0x07;

        uint8_t duzeltilmis_alt = Hamming74_HataDuzelt(alt_nibble, p_alt_orijinal, &toplam_hata);
        uint8_t duzeltilmis_ust = Hamming74_HataDuzelt(ust_nibble, p_ust_orijinal, &toplam_hata);

        guvenli_anahtar[i] = (duzeltilmis_ust << 4) | duzeltilmis_alt;
    }

    return toplam_hata;
}
