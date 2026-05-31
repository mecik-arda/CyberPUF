#ifndef AI_INFERENCE_H
#define AI_INFERENCE_H

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

void Conv2D_3x3_Same(const float* input, float* output, const ConvLayerParams* params, 
                    int in_h, int in_w, int in_c, int out_c);

void BatchNorm_ReLU(float* data, const ConvLayerParams* params, int h, int w, int c);

void MaxPool_2x2(const float* input, float* output, int in_h, int in_w, int c);

void Dense_Layer(const float* input, float* output, const DenseLayerParams* params, int in_features, int out_features);

void BatchNorm_ReLU_Dense(float* data, const DenseLayerParams* params, int features);

void Dense_Final_Softmax(const float* input, float* output, const DenseFinalParams* params, int in_features, int num_classes);

void Run_CypherPUF_CNN(const float* input_image, float* raw_weights, float* output_probs);

#endif
