#include "yardimci_veri_uretici.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "xtime_l.h"

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

// Minimal SHA-256 for 32-byte input (KDF)
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROR(x, 2) ^ ROR(x, 13) ^ ROR(x, 22))
#define EP1(x) (ROR(x, 6) ^ ROR(x, 11) ^ ROR(x, 25))
#define SIG0(x) (ROR(x, 7) ^ ROR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROR(x, 17) ^ ROR(x, 19) ^ ((x) >> 10))

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_kdf_32(const uint8_t *data, uint8_t hash[32]) {
    uint32_t state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    uint8_t block[64] = {0};
    uint32_t w[64];
    memcpy(block, data, 32);
    block[32] = 0x80;
    block[62] = 0x01;
    block[63] = 0x00; // 256 bits

    for (int i = 0; i < 16; i++) w[i] = (block[i*4]<<24) | (block[i*4+1]<<16) | (block[i*4+2]<<8) | block[i*4+3];
    for (int i = 16; i < 64; i++) w[i] = w[i-16] + SIG0(w[i-15]) + w[i-7] + SIG1(w[i-2]);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t temp1 = h + EP1(e) + CH(e, f, g) + k[i] + w[i];
        uint32_t temp2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    for (int i = 0; i < 8; i++) {
        hash[i*4] = (state[i] >> 24) & 0xFF;
        hash[i*4+1] = (state[i] >> 16) & 0xFF;
        hash[i*4+2] = (state[i] >> 8) & 0xFF;
        hash[i*4+3] = state[i] & 0xFF;
    }
}

void FuzzyExtractor_Kayit(const uint8_t* puf_ham_anahtar, YardimciVeri* yardimci_veri, uint8_t* guvenli_anahtar) {
    // 1. Rastgele bir guvenli anahtar (AES anahtari) uret.
    // PUF'tan bagimsiz, TRNG veya donanim saati destekli bir tohumlama kullanilmali
    XTime t;
    XTime_GetTime(&t);
    uint32_t seed = (uint32_t)t ^ 0x5AA5C3C3;
    srand(seed);
    for (int i = 0; i < 32; i++) {
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
