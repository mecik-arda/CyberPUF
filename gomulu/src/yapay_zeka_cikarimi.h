#ifndef YAPAY_ZEKA_CIKARIMI_H
#define YAPAY_ZEKA_CIKARIMI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float* w;
    float* b;
    float* gamma;
    float* beta;
    float* mean;
    float* var;
} ConvLayerParams;

typedef struct {
    float* w;
    float* b;
    float* gamma;
    float* beta;
    float* mean;
    float* var;
} DenseLayerParams;

typedef struct {
    float* w;
    float* b;
} DenseFinalParams;

void Conv2D_3x3_Same(const float* giris, float* cikis, const ConvLayerParams* params, 
                    int giris_y, int giris_g, int giris_k, int cikis_k);

void BatchNorm_ReLU(float* data, const ConvLayerParams* params, int h, int w, int c);

void MaxPool_2x2(const float* giris, float* cikis, int giris_y, int giris_g, int c);

void Dense_Layer(const float* giris, float* cikis, const DenseLayerParams* params, int giris_ozellikleri, int cikis_ozellikleri);

void BatchNorm_ReLU_Dense(float* data, const DenseLayerParams* params, int ozellikler);

void Dense_Final_Softmax(const float* giris, float* cikis, const DenseFinalParams* params, int giris_ozellikleri, int sinif_sayisi);

void CypherPUF_CNN_Calistir(const float* giris_goruntusu, float* ham_agirliklar, float* cikis_olasiliklari);

#endif
