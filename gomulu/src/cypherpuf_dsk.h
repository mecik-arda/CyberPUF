#ifndef CYPHERPUF_DSK_H
#define CYPHERPUF_DSK_H

#include <stdint.h>
#include <stdbool.h>

#define CYPHERPUF_REG_KONTROL      0x00
#define CYPHERPUF_REG_DURUM       0x04
#define CYPHERPUF_REG_VERI_GIRIS_0    0x08
#define CYPHERPUF_REG_VERI_GIRIS_1    0x0C
#define CYPHERPUF_REG_VERI_GIRIS_2    0x10
#define CYPHERPUF_REG_VERI_GIRIS_3    0x14
#define CYPHERPUF_REG_VERI_CIKIS_0   0x18
#define CYPHERPUF_REG_VERI_CIKIS_1   0x1C
#define CYPHERPUF_REG_VERI_CIKIS_2   0x20
#define CYPHERPUF_REG_VERI_CIKIS_3   0x24
#define CYPHERPUF_REG_PUF_ANAHTAR_0    0x28
#define CYPHERPUF_REG_HATA_AYIKLAMA_0      0x48
#define CYPHERPUF_REG_HATA_AYIKLAMA_1      0x4C

#define KONTROL_ANAHTAR_URET_BITI      (1 << 0)
#define KONTROL_SIFRE_COZ_BASLA_BITI     (1 << 1)
#define KONTROL_DURUM_TEMIZLE_BITI      (1 << 4)

#define DURUM_PUF_MESGUL_BITI        (1 << 0)
#define DURUM_PUF_TAMAM_BITI        (1 << 1)
#define DURUM_ANAHTAR_GEN_MESGUL_BITI       (1 << 2)
#define DURUM_ANAHTAR_GEN_TAMAM_BITI       (1 << 3)
#define DURUM_AES_MESGUL_BITI        (1 << 4)
#define DURUM_AES_TAMAM_BITI        (1 << 5)

void CypherPUF_Baslat(uint32_t taban_adresi);

bool CypherPUF_AnahtarUret(void);

bool CypherPUF_BlokSifreCoz(const uint8_t* sifreli_metin_16b, uint8_t* duz_metin_16b);

void CypherPUF_TamponSifreCoz(const uint8_t* sifreli_metin, uint8_t* duz_metin, uint32_t boyut_bayt);

void CypherPUF_PUFAnahtariAl(uint8_t* anahtar_tamponu_32b);

uint32_t CypherPUF_DurumAl(void);

#endif
