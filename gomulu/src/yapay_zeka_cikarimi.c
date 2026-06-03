#include "yapay_zeka_cikarimi.h"
#include <math.h>
#include "xil_printf.h"
#include <string.h>

#include <stdlib.h>

#define MAKS_TAMPON_BOYUTU (32 * 32 * 256)

static float* tampon1 = NULL;
static float* tampon2 = NULL;

void Conv2D_3x3_Same(const float* giris, float* cikis, const ConvLayerParams* params, int giris_y, int giris_g, int giris_k, int cikis_k) {
    for (int h = 0; h < giris_y; h++) {
        for (int w = 0; w < giris_g; w++) {
            for (int ck = 0; ck < cikis_k; ck++) {
                cikis[(h * giris_g + w) * cikis_k + ck] = params->b[ck];
            }
        }
    }
    
    for (int cy = -1; cy <= 1; cy++) {
        for (int cg = -1; cg <= 1; cg++) {
            int cy_idx = (cy + 1) * 3 + (cg + 1);
            int w_offset = cy_idx * giris_k * cikis_k;
            
            for (int h = 0; h < giris_y; h++) {
                int r = h + cy;
                if (r < 0 || r >= giris_y) continue;
                
                for (int w = 0; w < giris_g; w++) {
                    int c = w + cg;
                    if (c < 0 || c >= giris_g) continue;
                    
                    int g_base = (r * giris_g + c) * giris_k;
                    int out_idx = (h * giris_g + w) * cikis_k;
                    
                    for (int gk = 0; gk < giris_k; gk++) {
                        float g_val = giris[g_base + gk];
                        int w_base = w_offset + gk * cikis_k;
                        
                        for (int ck = 0; ck < cikis_k; ck++) {
                            cikis[out_idx + ck] += g_val * params->w[w_base + ck];
                        }
                    }
                }
            }
        }
    }
}

float* CPUF_Ikilisi_Ayristir(uint8_t* cozulmus_veri, uint32_t toplam_boyut) {
    uint32_t ofset = 0;
    
    if (toplam_boyut < 16) {
        xil_printf("HATA: Veri boyutu cok kucuk.\n");
        return NULL;
    }
    
    if (cozulmus_veri[0] != 'C' || cozulmus_veri[1] != 'P' || cozulmus_veri[2] != 'U' || cozulmus_veri[3] != 'F') {
        xil_printf("HATA: Gecersiz CPUF sihirli numarasi.\n");
        return NULL;
    }
    ofset += 4;
    
    uint8_t ver_major = cozulmus_veri[ofset++];
    uint8_t ver_minor = cozulmus_veri[ofset++];
    if (ver_major != 1) return NULL;
    
    uint32_t toplam_diziler;
    memcpy(&toplam_diziler, &cozulmus_veri[ofset], sizeof(uint32_t));
    ofset += 4;
    
    if (toplam_diziler > 100) return NULL;
    
    uint64_t toplam_elemanlar;
    memcpy(&toplam_elemanlar, &cozulmus_veri[ofset], sizeof(uint64_t));
    ofset += 8;
    
    if (toplam_elemanlar * sizeof(float) > toplam_boyut - ofset) return NULL;
    
    if (ofset + 16 > toplam_boyut) return NULL;
    ofset += 16;
    
    for (uint32_t i = 0; i < toplam_diziler; i++) {
        if (ofset >= toplam_boyut) return NULL;
        uint8_t ndim = cozulmus_veri[ofset++];

        // Integer overflow ve sinir disi erisim kontrolu
        if (ndim > (toplam_boyut - ofset) / 4) return NULL;
        ofset += ndim * 4;

        if (ofset + 8 > toplam_boyut) return NULL;
        ofset += 8;
    }
    
    if (ofset >= toplam_boyut) return NULL;
    
    if (ofset % sizeof(float) != 0) return NULL;
    return (float*)&cozulmus_veri[ofset];
}

void BatchNorm_ReLU(float* data, const ConvLayerParams* params, int h, int w, int c) {
    float epsilon = 1e-3f;
    for (int i = 0; i < h * w; i++) {
        for (int ch = 0; ch < c; ch++) {
            float deger = data[i * c + ch];
            float m = params->mean[ch];
            float v = params->var[ch];
            float gamma = params->gamma[ch];
            float beta = params->beta[ch];
            
            deger = gamma * (deger - m) / sqrtf(fabsf(v) + epsilon) + beta;
            
            if (deger < 0.0f) {
                deger = 0.0f;
            }
            data[i * c + ch] = deger;
        }
    }
}

void MaxPool_2x2(const float* giris, float* cikis, int giris_y, int giris_g, int c) {
    int out_h = giris_y / 2;
    int out_w = giris_g / 2;
    
    for (int h = 0; h < out_h; h++) {
        for (int w = 0; w < out_w; w++) {
            for (int ch = 0; ch < c; ch++) {
                float maks_deger = -1e6f;
                for (int cy = 0; cy < 2; cy++) {
                    for (int cg = 0; cg < 2; cg++) {
                        int r = h * 2 + cy;
                        int cl = w * 2 + cg;
                        float deger = giris[(r * giris_g + cl) * c + ch];
                        if (deger > maks_deger) {
                            maks_deger = deger;
                        }
                    }
                }
                cikis[(h * out_w + w) * c + ch] = maks_deger;
            }
        }
    }
}

void Dense_Layer(const float* giris, float* cikis, const DenseLayerParams* params, int giris_ozellikleri, int cikis_ozellikleri) {
    for (int o = 0; o < cikis_ozellikleri; o++) {
        float toplam = params->b[o];
        for (int i = 0; i < giris_ozellikleri; i++) {
            toplam += giris[i] * params->w[i * cikis_ozellikleri + o];
        }
        cikis[o] = toplam;
    }
}

void BatchNorm_ReLU_Dense(float* data, const DenseLayerParams* params, int ozellikler) {
    float epsilon = 1e-3f;
    for (int i = 0; i < ozellikler; i++) {
        float deger = data[i];
        float m = params->mean[i];
        float v = params->var[i];
        float gamma = params->gamma[i];
        float beta = params->beta[i];
        
        deger = gamma * (deger - m) / sqrtf(fabsf(v) + epsilon) + beta;
        
        if (deger < 0.0f) {
            deger = 0.0f;
        }
        data[i] = deger;
    }
}

void Dense_Final_Softmax(const float* giris, float* cikis, const DenseFinalParams* params, int giris_ozellikleri, int sinif_sayisi) {
    float maks_deger = -1e6f;
    for (int o = 0; o < sinif_sayisi; o++) {
        float toplam = params->b[o];
        for (int i = 0; i < giris_ozellikleri; i++) {
            toplam += giris[i] * params->w[i * sinif_sayisi + o];
        }
        cikis[o] = toplam;
        if (toplam > maks_deger) {
            maks_deger = toplam;
        }
    }
    
    float toplam_ustel = 0.0f;
    for (int o = 0; o < sinif_sayisi; o++) {
        cikis[o] = expf(cikis[o] - maks_deger);
        toplam_ustel += cikis[o];
    }
    
    for (int o = 0; o < sinif_sayisi; o++) {
        cikis[o] /= toplam_ustel;
    }
}

static float* ConvParametreleriniCikar(float* ptr, ConvLayerParams* p, int giris_k, int cikis_k, uint32_t* kapasite) {
    uint32_t gereken = (3 * 3 * giris_k * cikis_k) + 5 * cikis_k;
    if (*kapasite < gereken) return NULL;
    *kapasite -= gereken;
    p->w = ptr; ptr += (3 * 3 * giris_k * cikis_k);
    p->b = ptr; ptr += cikis_k;
    p->gamma = ptr; ptr += cikis_k;
    p->beta = ptr; ptr += cikis_k;
    p->mean = ptr; ptr += cikis_k;
    p->var = ptr; ptr += cikis_k;
    return ptr;
}

static float* DenseParametreleriniCikar(float* ptr, DenseLayerParams* p, int in_f, int out_f, uint32_t* kapasite) {
    uint32_t gereken = (in_f * out_f) + 5 * out_f;
    if (*kapasite < gereken) return NULL;
    *kapasite -= gereken;
    p->w = ptr; ptr += (in_f * out_f);
    p->b = ptr; ptr += out_f;
    p->gamma = ptr; ptr += out_f;
    p->beta = ptr; ptr += out_f;
    p->mean = ptr; ptr += out_f;
    p->var = ptr; ptr += out_f;
    return ptr;
}

void CyberPUF_CNN_Calistir(const float* giris_goruntusu, float* ham_agirliklar, uint32_t agirlik_kapasitesi, float* cikis_olasiliklari) {
    float* w_ptr = ham_agirliklar;
    ConvLayerParams conv1_1, conv1_2, conv2_1, conv2_2, conv3_1, conv3_2;
    DenseLayerParams dense1, dense2;
    DenseFinalParams final_dense;

    tampon1 = (float*)malloc(MAKS_TAMPON_BOYUTU * sizeof(float));
    tampon2 = (float*)malloc(MAKS_TAMPON_BOYUTU * sizeof(float));
    if (!tampon1 || !tampon2) {
        xil_printf("HATA: tampon1/tampon2 malloc basarisiz.\n");
        if(tampon1) free(tampon1);
        if(tampon2) free(tampon2);
        return;
    }

    uint32_t kalan_kapasite = agirlik_kapasitesi;

    w_ptr = ConvParametreleriniCikar(w_ptr, &conv1_1, 3, 64, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = ConvParametreleriniCikar(w_ptr, &conv1_2, 64, 64, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = ConvParametreleriniCikar(w_ptr, &conv2_1, 64, 128, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = ConvParametreleriniCikar(w_ptr, &conv2_2, 128, 128, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = ConvParametreleriniCikar(w_ptr, &conv3_1, 128, 256, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = ConvParametreleriniCikar(w_ptr, &conv3_2, 256, 256, &kalan_kapasite);
    if (!w_ptr) goto coker;

    w_ptr = DenseParametreleriniCikar(w_ptr, &dense1, 4096, 512, &kalan_kapasite);
    if (!w_ptr) goto coker;
    w_ptr = DenseParametreleriniCikar(w_ptr, &dense2, 512, 256, &kalan_kapasite);
    if (!w_ptr) goto coker;

    if (kalan_kapasite < (256 * 10) + 10) goto coker;
    final_dense.w = w_ptr; w_ptr += (256 * 10);
    final_dense.b = w_ptr; w_ptr += 10;

    Conv2D_3x3_Same(giris_goruntusu, tampon1, &conv1_1, 32, 32, 3, 64);
    BatchNorm_ReLU(tampon1, &conv1_1, 32, 32, 64);

    Conv2D_3x3_Same(tampon1, tampon2, &conv1_2, 32, 32, 64, 64);
    BatchNorm_ReLU(tampon2, &conv1_2, 32, 32, 64);

    MaxPool_2x2(tampon2, tampon1, 32, 32, 64);

    Conv2D_3x3_Same(tampon1, tampon2, &conv2_1, 16, 16, 64, 128);
    BatchNorm_ReLU(tampon2, &conv2_1, 16, 16, 128);

    Conv2D_3x3_Same(tampon2, tampon1, &conv2_2, 16, 16, 128, 128);
    BatchNorm_ReLU(tampon1, &conv2_2, 16, 16, 128);

    MaxPool_2x2(tampon1, tampon2, 16, 16, 128);

    Conv2D_3x3_Same(tampon2, tampon1, &conv3_1, 8, 8, 128, 256);
    BatchNorm_ReLU(tampon1, &conv3_1, 8, 8, 256);

    Conv2D_3x3_Same(tampon1, tampon2, &conv3_2, 8, 8, 256, 256);
    BatchNorm_ReLU(tampon2, &conv3_2, 8, 8, 256);

    MaxPool_2x2(tampon2, tampon1, 8, 8, 256);

    Dense_Layer(tampon1, tampon2, &dense1, 4096, 512);
    BatchNorm_ReLU_Dense(tampon2, &dense1, 512);

    Dense_Layer(tampon2, tampon1, &dense2, 512, 256);
    BatchNorm_ReLU_Dense(tampon1, &dense2, 256);

    Dense_Final_Softmax(tampon1, cikis_olasiliklari, &final_dense, 256, 10);
    
    free(tampon1);
    free(tampon2);
    tampon1 = NULL;
    tampon2 = NULL;
    return;

coker:
    xil_printf("HATA: Agirlik bellegi sinir disi (OOB read) engellendi.\n");
    if(tampon1) free(tampon1);
    if(tampon2) free(tampon2);
    tampon1 = NULL;
    tampon2 = NULL;
}
