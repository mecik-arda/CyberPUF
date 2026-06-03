#include "yardimci_veri_uretici.h"
#include <stdlib.h>
#include <time.h>

// Extended Hamming(8,4) Parite Hesaplama (4 bit dondurur: p_genel, p3, p2, p1)
static uint8_t Hamming84_PariteHesapla(uint8_t d) {
    uint8_t d1 = d & 1;
    uint8_t d2 = (d >> 1) & 1;
    uint8_t d3 = (d >> 2) & 1;
    uint8_t d4 = (d >> 3) & 1;

    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p3 = d2 ^ d3 ^ d4;
    
    // p_genel is XOR of all data bits and parity bits
    uint8_t p_genel = d1 ^ d2 ^ d3 ^ d4 ^ p1 ^ p2 ^ p3;

    return (p_genel << 3) | (p3 << 2) | (p2 << 1) | p1;
}

// Extended Hamming(8,4) Hata Duzeltme (SECDED)
static int Hamming84_HataDuzelt(uint8_t gurultulu_d, uint8_t orijinal_p, uint8_t* duzeltilmis_d, int* hata_sayisi) {
    uint8_t yeni_p = Hamming84_PariteHesapla(gurultulu_d);
    
    uint8_t sendrom = (yeni_p ^ orijinal_p) & 0x07; // Alt 3 bit (p1, p2, p3 sendromu)
    uint8_t p_genel_fark = ((yeni_p ^ orijinal_p) >> 3) & 0x01;
    
    if (sendrom == 0 && p_genel_fark == 0) {
        *duzeltilmis_d = gurultulu_d;
        return 0; // Hata yok
    }
    
    if (sendrom != 0 && p_genel_fark == 0) {
        return -1; // Cift bit hata tespit edildi, duzeltme yapma
    }
    
    if (hata_sayisi) {
        (*hata_sayisi)++;
    }

    if (sendrom == 0 && p_genel_fark == 1) {
        *duzeltilmis_d = gurultulu_d; // Sadece genel paritede hata var, veri dogru
        return 0;
    }

    uint8_t d_kopya = gurultulu_d;
    if (sendrom == 3)      d_kopya ^= 0x1;
    else if (sendrom == 5) d_kopya ^= 0x2;
    else if (sendrom == 6) d_kopya ^= 0x4;
    else if (sendrom == 7) d_kopya ^= 0x8;
    // sendrom 1, 2, 4 ise p1, p2, p3 bitlerinden birinde hata olmustur, veri dogrudur
    
    *duzeltilmis_d = d_kopya & 0x0F;
    return 0;
}

void FuzzyExtractor_Kayit(const uint8_t* puf_ham_anahtar, YardimciVeri* yardimci_veri, uint8_t* guvenli_anahtar) {
    // 1. Rastgele bir guvenli anahtar (AES anahtari) uret.
    // Tum 32 bayti rotation-XOR ile harmanlayarak entropi kaybi onlenir.
    uint32_t seed_val = 0;
    for (int i = 0; i < 32; i++) {
        seed_val ^= ((uint32_t)puf_ham_anahtar[i]) << ((i % 4) * 8);
    }
    srand(seed_val);
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        guvenli_anahtar[i] = rand() & 0xFF;
    }

    // 2. Helper Data 1: Code-Offset XOR Maskesi
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        yardimci_veri->xor_maskesi[i] = puf_ham_anahtar[i] ^ guvenli_anahtar[i];
    }

    // 3. Helper Data 2: Extended Hamming(8,4) Parite Verisi
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        uint8_t alt_nibble = guvenli_anahtar[i] & 0x0F;
        uint8_t ust_nibble = (guvenli_anahtar[i] >> 4) & 0x0F;

        uint8_t p_alt = Hamming84_PariteHesapla(alt_nibble);
        uint8_t p_ust = Hamming84_PariteHesapla(ust_nibble);

        // 8 biti tek bir byte icine yerlestir: [P_ust(4) P_alt(4)]
        yardimci_veri->parite_verisi[i] = (p_ust << 4) | p_alt;
    }
}

int FuzzyExtractor_Cikarim(const uint8_t* puf_yeni_anahtar, const YardimciVeri* yardimci_veri, uint8_t* guvenli_anahtar) {
    int toplam_hata = 0;

    // 1. Code-Offset ile gurultulu (aday) anahtari elde et
    for (int i = 0; i < PUF_ANAHTAR_BOYUTU; i++) {
        uint8_t gurultulu_bayt = puf_yeni_anahtar[i] ^ yardimci_veri->xor_maskesi[i];

        // 2. Extended Hamming(8,4) paritesi ile varsa bit hatalarini duzelt
        uint8_t alt_nibble = gurultulu_bayt & 0x0F;
        uint8_t ust_nibble = (gurultulu_bayt >> 4) & 0x0F;

        uint8_t p_alt_orijinal = yardimci_veri->parite_verisi[i] & 0x0F;
        uint8_t p_ust_orijinal = (yardimci_veri->parite_verisi[i] >> 4) & 0x0F;

        uint8_t duzeltilmis_alt, duzeltilmis_ust;
        if (Hamming84_HataDuzelt(alt_nibble, p_alt_orijinal, &duzeltilmis_alt, &toplam_hata) != 0) {
            return -1; // Cift bit hata
        }
        if (Hamming84_HataDuzelt(ust_nibble, p_ust_orijinal, &duzeltilmis_ust, &toplam_hata) != 0) {
            return -1; // Cift bit hata
        }

        guvenli_anahtar[i] = (duzeltilmis_ust << 4) | duzeltilmis_alt;
    }

    return toplam_hata;
}
