#include "ai_inference.h"
#include <math.h>

#define MAX_BUFFER_SIZE (32 * 32 * 256)

static float buffer1[MAX_BUFFER_SIZE];
static float buffer2[MAX_BUFFER_SIZE];

void Conv2D_3x3_Same(const float* input, float* output, const ConvLayerParams* params, int in_h, int in_w, int in_c, int out_c) {
    for (int h = 0; h < in_h; h++) {
        for (int w = 0; w < in_w; w++) {
            for (int oc = 0; oc < out_c; oc++) {
                float sum = params->b[oc];
                for (int kh = -1; kh <= 1; kh++) {
                    for (int kw = -1; kw <= 1; kw++) {
                        int r = h + kh;
                        int c = w + kw;
                        if (r >= 0 && r < in_h && c >= 0 && c < in_w) {
                            for (int ic = 0; ic < in_c; ic++) {
                                int w_idx = ((kh + 1) * 3 + (kw + 1)) * (in_c * out_c) + ic * out_c + oc;
                                int i_idx = (r * in_w + c) * in_c + ic;
                                sum += input[i_idx] * params->w[w_idx];
                            }
                        }
                    }
                }
                output[(h * in_w + w) * out_c + oc] = sum;
            }
        }
    }
}

void BatchNorm_ReLU(float* data, const ConvLayerParams* params, int h, int w, int c) {
    float epsilon = 1e-3f;
    for (int i = 0; i < h * w; i++) {
        for (int ch = 0; ch < c; ch++) {
            float val = data[i * c + ch];
            float m = params->mean[ch];
            float v = params->var[ch];
            float gamma = params->gamma[ch];
            float beta = params->beta[ch];
            
            val = gamma * (val - m) / sqrtf(v + epsilon) + beta;
            
            if (val < 0.0f) {
                val = 0.0f;
            }
            data[i * c + ch] = val;
        }
    }
}

void MaxPool_2x2(const float* input, float* output, int in_h, int in_w, int c) {
    int out_h = in_h / 2;
    int out_w = in_w / 2;
    
    for (int h = 0; h < out_h; h++) {
        for (int w = 0; w < out_w; w++) {
            for (int ch = 0; ch < c; ch++) {
                float max_val = -1e6f;
                for (int kh = 0; kh < 2; kh++) {
                    for (int kw = 0; kw < 2; kw++) {
                        int r = h * 2 + kh;
                        int cl = w * 2 + kw;
                        float val = input[(r * in_w + cl) * c + ch];
                        if (val > max_val) {
                            max_val = val;
                        }
                    }
                }
                output[(h * out_w + w) * c + ch] = max_val;
            }
        }
    }
}

void Dense_Layer(const float* input, float* output, const DenseLayerParams* params, int in_features, int out_features) {
    for (int o = 0; o < out_features; o++) {
        float sum = params->b[o];
        for (int i = 0; i < in_features; i++) {
            sum += input[i] * params->w[i * out_features + o];
        }
        output[o] = sum;
    }
}

void BatchNorm_ReLU_Dense(float* data, const DenseLayerParams* params, int features) {
    float epsilon = 1e-3f;
    for (int i = 0; i < features; i++) {
        float val = data[i];
        float m = params->mean[i];
        float v = params->var[i];
        float gamma = params->gamma[i];
        float beta = params->beta[i];
        
        val = gamma * (val - m) / sqrtf(v + epsilon) + beta;
        
        if (val < 0.0f) {
            val = 0.0f;
        }
        data[i] = val;
    }
}

void Dense_Final_Softmax(const float* input, float* output, const DenseFinalParams* params, int in_features, int num_classes) {
    float max_val = -1e6f;
    for (int o = 0; o < num_classes; o++) {
        float sum = params->b[o];
        for (int i = 0; i < in_features; i++) {
            sum += input[i] * params->w[i * num_classes + o];
        }
        output[o] = sum;
        if (sum > max_val) {
            max_val = sum;
        }
    }
    
    float sum_exp = 0.0f;
    for (int o = 0; o < num_classes; o++) {
        output[o] = expf(output[o] - max_val);
        sum_exp += output[o];
    }
    
    for (int o = 0; o < num_classes; o++) {
        output[o] /= sum_exp;
    }
}

static float* ExtractConvParams(float* ptr, ConvLayerParams* p, int in_c, int out_c) {
    p->w = ptr; ptr += (3 * 3 * in_c * out_c);
    p->b = ptr; ptr += out_c;
    p->gamma = ptr; ptr += out_c;
    p->beta = ptr; ptr += out_c;
    p->mean = ptr; ptr += out_c;
    p->var = ptr; ptr += out_c;
    return ptr;
}

static float* ExtractDenseParams(float* ptr, DenseLayerParams* p, int in_f, int out_f) {
    p->w = ptr; ptr += (in_f * out_f);
    p->b = ptr; ptr += out_f;
    p->gamma = ptr; ptr += out_f;
    p->beta = ptr; ptr += out_f;
    p->mean = ptr; ptr += out_f;
    p->var = ptr; ptr += out_f;
    return ptr;
}

void Run_CypherPUF_CNN(const float* input_image, float* raw_weights, float* output_probs) {
    float* w_ptr = raw_weights;
    ConvLayerParams conv1_1, conv1_2, conv2_1, conv2_2, conv3_1, conv3_2;
    DenseLayerParams dense1, dense2;
    DenseFinalParams final_dense;

    w_ptr = ExtractConvParams(w_ptr, &conv1_1, 3, 64);
    w_ptr = ExtractConvParams(w_ptr, &conv1_2, 64, 64);
    w_ptr = ExtractConvParams(w_ptr, &conv2_1, 64, 128);
    w_ptr = ExtractConvParams(w_ptr, &conv2_2, 128, 128);
    w_ptr = ExtractConvParams(w_ptr, &conv3_1, 128, 256);
    w_ptr = ExtractConvParams(w_ptr, &conv3_2, 256, 256);

    w_ptr = ExtractDenseParams(w_ptr, &dense1, 4096, 512);
    w_ptr = ExtractDenseParams(w_ptr, &dense2, 512, 256);

    final_dense.w = w_ptr; w_ptr += (256 * 10);
    final_dense.b = w_ptr; w_ptr += 10;

    Conv2D_3x3_Same(input_image, buffer1, &conv1_1, 32, 32, 3, 64);
    BatchNorm_ReLU(buffer1, &conv1_1, 32, 32, 64);

    Conv2D_3x3_Same(buffer1, buffer2, &conv1_2, 32, 32, 64, 64);
    BatchNorm_ReLU(buffer2, &conv1_2, 32, 32, 64);

    MaxPool_2x2(buffer2, buffer1, 32, 32, 64);

    Conv2D_3x3_Same(buffer1, buffer2, &conv2_1, 16, 16, 64, 128);
    BatchNorm_ReLU(buffer2, &conv2_1, 16, 16, 128);

    Conv2D_3x3_Same(buffer2, buffer1, &conv2_2, 16, 16, 128, 128);
    BatchNorm_ReLU(buffer1, &conv2_2, 16, 16, 128);

    MaxPool_2x2(buffer1, buffer2, 16, 16, 128);

    Conv2D_3x3_Same(buffer2, buffer1, &conv3_1, 8, 8, 128, 256);
    BatchNorm_ReLU(buffer1, &conv3_1, 8, 8, 256);

    Conv2D_3x3_Same(buffer1, buffer2, &conv3_2, 8, 8, 256, 256);
    BatchNorm_ReLU(buffer2, &conv3_2, 8, 8, 256);

    MaxPool_2x2(buffer2, buffer1, 8, 8, 256);

    Dense_Layer(buffer1, buffer2, &dense1, 4096, 512);
    BatchNorm_ReLU_Dense(buffer2, &dense1, 512);

    Dense_Layer(buffer2, buffer1, &dense2, 512, 256);
    BatchNorm_ReLU_Dense(buffer1, &dense2, 256);

    Dense_Final_Softmax(buffer1, output_probs, &final_dense, 256, 10);
}
