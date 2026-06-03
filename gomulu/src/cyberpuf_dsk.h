#ifndef CYBERPUF_DSK_H
#define CYBERPUF_DSK_H

#include <stdint.h>
#include <stdbool.h>

#define CYBERPUF_REG_KONTROL      0x00
#define CYBERPUF_REG_DURUM       0x04
#define CYBERPUF_REG_VERI_GIRIS_0    0x08
#define CYBERPUF_REG_VERI_GIRIS_1    0x0C
#define CYBERPUF_REG_VERI_GIRIS_2    0x10
#define CYBERPUF_REG_VERI_GIRIS_3    0x14
#define CYBERPUF_REG_VERI_CIKIS_0   0x18
#define CYBERPUF_REG_VERI_CIKIS_1   0x1C
#define CYBERPUF_REG_VERI_CIKIS_2   0x20
#define CYBERPUF_REG_VERI_CIKIS_3   0x24
#define CYBERPUF_REG_PUF_ANAHTAR_0    0x28
#define CYBERPUF_REG_HATA_AYIKLAMA_0      0x48
#define CYBERPUF_REG_HATA_AYIKLAMA_1      0x4C

#define KONTROL_ANAHTAR_URET_BITI      (1 << 0)
#define KONTROL_SIFRE_COZ_BASLA_BITI     (1 << 1)
#define KONTROL_DURUM_TEMIZLE_BITI      (1 << 4)

#define DURUM_PUF_MESGUL_BITI        (1 << 0)
#define DURUM_PUF_TAMAM_BITI        (1 << 1)
#define DURUM_ANAHTAR_GEN_MESGUL_BITI       (1 << 2)
#define DURUM_ANAHTAR_GEN_TAMAM_BITI       (1 << 3)
#define DURUM_AES_MESGUL_BITI        (1 << 4)
#define DURUM_AES_TAMAM_BITI        (1 << 5)

void CyberPUF_Baslat(uint32_t taban_adresi);

bool CyberPUF_AnahtarUret(void);

bool CyberPUF_BlokSifreCoz(const uint8_t* sifreli_metin_16b, uint8_t* duz_metin_16b);

bool CyberPUF_TamponSifreCoz(const uint8_t* sifreli_metin, uint8_t* duz_metin, uint32_t boyut_bayt, const uint8_t* iv);

void CyberPUF_PUFAnahtariAl(uint8_t* anahtar_tamponu_32b);

uint32_t CyberPUF_DurumAl(void);

#endif
