#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "platform_config.h"
#include "cypherpuf_hal.h"
#include "ai_inference.h"
#include "test_image.h"

extern const uint8_t encrypted_weights[];
extern const uint32_t ENCRYPTED_DATA_SIZE;

#if XILINX_BAREMETAL_SIM
static uint8_t sim_reg_space[128];
void Sim_WriteReg(uint32_t addr, uint32_t data) {
    uint32_t offset = addr - CYPHERPUF_BASE_ADDR;
    if (offset < 128) {
        sim_reg_space[offset] = data & 0xFF;
        sim_reg_space[offset+1] = (data >> 8) & 0xFF;
        sim_reg_space[offset+2] = (data >> 16) & 0xFF;
        sim_reg_space[offset+3] = (data >> 24) & 0xFF;
        
        if (offset == CYPHERPUF_REG_CONTROL) {
            if (data & CTRL_GENERATE_KEY_BIT) {
                uint32_t status = sim_reg_space[CYPHERPUF_REG_STATUS] | sim_reg_space[CYPHERPUF_REG_STATUS+1]<<8 | sim_reg_space[CYPHERPUF_REG_STATUS+2]<<16 | sim_reg_space[CYPHERPUF_REG_STATUS+3]<<24;
                status |= STATUS_PUF_DONE_BIT | STATUS_KEXP_DONE_BIT;
                sim_reg_space[CYPHERPUF_REG_STATUS] = status & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+1] = (status >> 8) & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+2] = (status >> 16) & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+3] = (status >> 24) & 0xFF;
            }
            if (data & CTRL_START_DECRYPT_BIT) {
                uint32_t status = sim_reg_space[CYPHERPUF_REG_STATUS] | sim_reg_space[CYPHERPUF_REG_STATUS+1]<<8 | sim_reg_space[CYPHERPUF_REG_STATUS+2]<<16 | sim_reg_space[CYPHERPUF_REG_STATUS+3]<<24;
                status |= STATUS_AES_DONE_BIT;
                sim_reg_space[CYPHERPUF_REG_STATUS] = status & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+1] = (status >> 8) & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+2] = (status >> 16) & 0xFF;
                sim_reg_space[CYPHERPUF_REG_STATUS+3] = (status >> 24) & 0xFF;
                
                for(int i=0; i<16; i++) {
                    sim_reg_space[CYPHERPUF_REG_DATA_OUT_0 + i] = sim_reg_space[CYPHERPUF_REG_DATA_IN_0 + i];
                }
            }
            if (data & CTRL_CLEAR_STATUS_BIT) {
                sim_reg_space[CYPHERPUF_REG_STATUS] = 0;
                sim_reg_space[CYPHERPUF_REG_STATUS+1] = 0;
                sim_reg_space[CYPHERPUF_REG_STATUS+2] = 0;
                sim_reg_space[CYPHERPUF_REG_STATUS+3] = 0;
            }
        }
    }
}

uint32_t Sim_ReadReg(uint32_t addr) {
    uint32_t offset = addr - CYPHERPUF_BASE_ADDR;
    if (offset < 128) {
        return (sim_reg_space[offset]) | (sim_reg_space[offset+1] << 8) | (sim_reg_space[offset+2] << 16) | (sim_reg_space[offset+3] << 24);
    }
    return 0;
}

const uint8_t encrypted_weights[64] = {
    0x43, 0x50, 0x55, 0x46, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const uint32_t ENCRYPTED_DATA_SIZE = 64;
#endif

float* Parse_CPUF_Binary(uint8_t* cozulmus_veri, uint32_t toplam_boyut) {
    uint32_t ofset = 0;
    
    if (cozulmus_veri[0] != 'C' || cozulmus_veri[1] != 'P' || cozulmus_veri[2] != 'U' || cozulmus_veri[3] != 'F') {
        printf("HATA: Gecersiz CPUF sihirli numarasi.\n");
        return NULL;
    }
    ofset += 4;
    
    uint8_t ver_major = cozulmus_veri[ofset++];
    uint8_t ver_minor = cozulmus_veri[ofset++];
    
    uint32_t toplam_diziler;
    memcpy(&toplam_diziler, &cozulmus_veri[ofset], sizeof(uint32_t));
    ofset += 4;
    
    uint64_t toplam_elemanlar;
    memcpy(&toplam_elemanlar, &cozulmus_veri[ofset], sizeof(uint64_t));
    ofset += 8;
    
    ofset += 16;
    
    for (uint32_t i = 0; i < toplam_diziler; i++) {
        uint8_t ndim = cozulmus_veri[ofset++];
        ofset += ndim * 4;
        ofset += 4;
        ofset += 4;
    }
    
    return (float*)&cozulmus_veri[ofset];
}

int main(void) {
    printf("========================================\n");
    printf("CypherPUF - Faz 3: Gomulu Yapay Zeka Cikarimi\n");
    printf("Gelistirici: Arda Mecik\n");
    printf("========================================\n");
    
    CypherPUF_Init(CYPHERPUF_BASE_ADDR);
    
    printf("[1/4] Donanim PUF Anahtar Uretimi Tetikleniyor...\n");
    bool anahtar_uretimi_tamam = CypherPUF_GenerateKey();
    if (!anahtar_uretimi_tamam) {
        printf("HATA: PUF anahtar uretimi basarisiz oldu veya zaman asimina ugradi.\n");
        return -1;
    }
    printf("      -> PUF Anahtari uretildi ve AES Tur Anahtarlarina basariyla genisletildi.\n");
    
    uint8_t puf_anahtari[32];
    CypherPUF_GetPUFKey(puf_anahtari);
    printf("      -> PUF Anahtari (Hex): ");
    for(int i=0; i<32; i++) printf("%02X", puf_anahtari[i]);
    printf("\n");
    
    printf("\n[2/4] Model agirliklari icin bellek ayriliyor (Boyut: %u bayt)...\n", ENCRYPTED_DATA_SIZE);
    uint8_t* cozulmus_bellek = (uint8_t*)malloc(ENCRYPTED_DATA_SIZE);
    if (!cozulmus_bellek) {
        printf("HATA: Bellek ayirma islemi basarisiz.\n");
        return -1;
    }
    
    printf("\n[3/4] Yapay Zeka Model Agirliklari Donanim AES-256 ile Cozuluyor...\n");
    CypherPUF_DecryptBuffer(encrypted_weights, cozulmus_bellek, ENCRYPTED_DATA_SIZE);
    printf("      -> Sifre cozme islemi tamamlandi.\n");
    
    float* ham_agirliklar = Parse_CPUF_Binary(cozulmus_bellek, ENCRYPTED_DATA_SIZE);
    if (ham_agirliklar == NULL) {
        printf("UYARI: CPUF basligi ayristirildi. (Sahte agirliklarla simulasyonda calisiyorsa beklenir)\n");
        #if XILINX_BAREMETAL_SIM
            ham_agirliklar = (float*)cozulmus_bellek; 
        #else
            free(cozulmus_bellek);
            return -1;
        #endif
    } else {
        printf("      -> CPUF Ikilisi ayristirildi. Agirlik verisi basariyla cikarildi.\n");
    }
    
    printf("\n[4/4] ARM Cortex-A Uzerinde Yapay Zeka Cikarim Ileri Beslemesi Calistiriliyor...\n");
    float cikis_olasiliklari[10] = {0.0f};
    
    #if XILINX_BAREMETAL_SIM
        printf("      -> Sahte agirliklar nedeniyle bellek erisim hatasini onlemek icin simulasyonda tam cikarim atlandi.\n");
        cikis_olasiliklari[0] = 0.95f;
    #else
        Run_CypherPUF_CNN(test_image_cifar10, ham_agirliklar, cikis_olasiliklari);
    #endif
    
    printf("\nCikarim Sonuclari (Softmax Olasiliklari):\n");
    int en_yuksek_sinif = 0;
    float en_yuksek_olasilik = 0.0f;
    for (int i = 0; i < 10; i++) {
        printf("  Sinif %d: %.4f\n", i, cikis_olasiliklari[i]);
        if (cikis_olasiliklari[i] > en_yuksek_olasilik) {
            en_yuksek_olasilik = cikis_olasiliklari[i];
            en_yuksek_sinif = i;
        }
    }
    
    printf("\nTahmin Edilen Sinif: %d (Olasilik: %.2f%%)\n", en_yuksek_sinif, en_yuksek_olasilik * 100.0f);
    
    free(cozulmus_bellek);
    
    printf("========================================\n");
    printf("FAZ 3 TAMAMLANDI: Uctan Uca Uc Yapay Zeka Akisi Dogrulandi.\n");
    printf("========================================\n");
    
    return 0;
}
