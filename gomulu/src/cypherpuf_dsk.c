#include "cypherpuf_dsk.h"
#include "platform_yapilandirmasi.h"

static uint32_t base_addr = CYPHERPUF_TABAN_ADRES;

void CypherPUF_Baslat(uint32_t taban_adresi) {
    base_addr = taban_adresi;
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, KONTROL_DURUM_TEMIZLE_BITI);
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, 0x00000000);
}

uint32_t CypherPUF_DurumAl(void) {
    return Xil_In32(base_addr + CYPHERPUF_REG_DURUM);
}

bool CypherPUF_AnahtarUret(void) {
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, KONTROL_DURUM_TEMIZLE_BITI);
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, 0x00000000);

    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, KONTROL_ANAHTAR_URET_BITI);
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, 0x00000000);

    uint32_t durum = 0;
    uint32_t timeout = 0xFFFFFF;

    while (timeout > 0) {
        durum = Xil_In32(base_addr + CYPHERPUF_REG_DURUM);
        if ((durum & DURUM_ANAHTAR_GEN_TAMAM_BITI) != 0) {
            return true;
        }
        timeout--;
    }

    return false;
}

bool CypherPUF_BlokSifreCoz(const uint8_t* sifreli_metin_16b, uint8_t* duz_metin_16b) {
    uint32_t w0 = ((uint32_t)sifreli_metin_16b[3] << 24) | ((uint32_t)sifreli_metin_16b[2] << 16) | ((uint32_t)sifreli_metin_16b[1] << 8) | sifreli_metin_16b[0];
    uint32_t w1 = ((uint32_t)sifreli_metin_16b[7] << 24) | ((uint32_t)sifreli_metin_16b[6] << 16) | ((uint32_t)sifreli_metin_16b[5] << 8) | sifreli_metin_16b[4];
    uint32_t w2 = ((uint32_t)sifreli_metin_16b[11] << 24) | ((uint32_t)sifreli_metin_16b[10] << 16) | ((uint32_t)sifreli_metin_16b[9] << 8) | sifreli_metin_16b[8];
    uint32_t w3 = ((uint32_t)sifreli_metin_16b[15] << 24) | ((uint32_t)sifreli_metin_16b[14] << 16) | ((uint32_t)sifreli_metin_16b[13] << 8) | sifreli_metin_16b[12];

    Xil_Out32(base_addr + CYPHERPUF_REG_VERI_GIRIS_0, w0);
    Xil_Out32(base_addr + CYPHERPUF_REG_VERI_GIRIS_1, w1);
    Xil_Out32(base_addr + CYPHERPUF_REG_VERI_GIRIS_2, w2);
    Xil_Out32(base_addr + CYPHERPUF_REG_VERI_GIRIS_3, w3);

    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, KONTROL_DURUM_TEMIZLE_BITI);
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, 0x00000000);

    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, KONTROL_SIFRE_COZ_BASLA_BITI);
    Xil_Out32(base_addr + CYPHERPUF_REG_KONTROL, 0x00000000);

    uint32_t durum = 0;
    uint32_t timeout = 0xFFFFFF;

    while (timeout > 0) {
        durum = Xil_In32(base_addr + CYPHERPUF_REG_DURUM);
        if ((durum & DURUM_AES_TAMAM_BITI) != 0) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        return false;
    }

    uint32_t r0 = Xil_In32(base_addr + CYPHERPUF_REG_VERI_CIKIS_0);
    uint32_t r1 = Xil_In32(base_addr + CYPHERPUF_REG_VERI_CIKIS_1);
    uint32_t r2 = Xil_In32(base_addr + CYPHERPUF_REG_VERI_CIKIS_2);
    uint32_t r3 = Xil_In32(base_addr + CYPHERPUF_REG_VERI_CIKIS_3);

    duz_metin_16b[0] = (uint8_t)(r0 & 0xFF);
    duz_metin_16b[1] = (uint8_t)((r0 >> 8) & 0xFF);
    duz_metin_16b[2] = (uint8_t)((r0 >> 16) & 0xFF);
    duz_metin_16b[3] = (uint8_t)((r0 >> 24) & 0xFF);

    duz_metin_16b[4] = (uint8_t)(r1 & 0xFF);
    duz_metin_16b[5] = (uint8_t)((r1 >> 8) & 0xFF);
    duz_metin_16b[6] = (uint8_t)((r1 >> 16) & 0xFF);
    duz_metin_16b[7] = (uint8_t)((r1 >> 24) & 0xFF);

    duz_metin_16b[8] = (uint8_t)(r2 & 0xFF);
    duz_metin_16b[9] = (uint8_t)((r2 >> 8) & 0xFF);
    duz_metin_16b[10] = (uint8_t)((r2 >> 16) & 0xFF);
    duz_metin_16b[11] = (uint8_t)((r2 >> 24) & 0xFF);

    duz_metin_16b[12] = (uint8_t)(r3 & 0xFF);
    duz_metin_16b[13] = (uint8_t)((r3 >> 8) & 0xFF);
    duz_metin_16b[14] = (uint8_t)((r3 >> 16) & 0xFF);
    duz_metin_16b[15] = (uint8_t)((r3 >> 24) & 0xFF);

    return true;
}

void CypherPUF_TamponSifreCoz(const uint8_t* sifreli_metin, uint8_t* duz_metin, uint32_t boyut_bayt) {
    uint32_t blocks = boyut_bayt / 16;
    for (uint32_t i = 0; i < blocks; i++) {
        CypherPUF_BlokSifreCoz(&sifreli_metin[i * 16], &duz_metin[i * 16]);
    }
}

void CypherPUF_PUFAnahtariAl(uint8_t* anahtar_tamponu_32b) {
    for (int i = 0; i < 8; i++) {
        uint32_t word = Xil_In32(base_addr + CYPHERPUF_REG_PUF_ANAHTAR_0 + (i * 4));
        anahtar_tamponu_32b[i * 4 + 0] = (uint8_t)(word & 0xFF);
        anahtar_tamponu_32b[i * 4 + 1] = (uint8_t)((word >> 8) & 0xFF);
        anahtar_tamponu_32b[i * 4 + 2] = (uint8_t)((word >> 16) & 0xFF);
        anahtar_tamponu_32b[i * 4 + 3] = (uint8_t)((word >> 24) & 0xFF);
    }
}
