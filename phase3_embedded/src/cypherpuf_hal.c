#include "cypherpuf_hal.h"
#include "platform_config.h"

static uint32_t base_addr = CYPHERPUF_BASE_ADDR;

void CypherPUF_Init(uint32_t base_address) {
    base_addr = base_address;
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, CTRL_CLEAR_STATUS_BIT);
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, 0x00000000);
}

uint32_t CypherPUF_GetStatus(void) {
    return Xil_In32(base_addr + CYPHERPUF_REG_STATUS);
}

bool CypherPUF_GenerateKey(void) {
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, CTRL_CLEAR_STATUS_BIT);
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, 0x00000000);

    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, CTRL_GENERATE_KEY_BIT);
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, 0x00000000);

    uint32_t status = 0;
    uint32_t timeout = 0xFFFFFF;

    while (timeout > 0) {
        status = Xil_In32(base_addr + CYPHERPUF_REG_STATUS);
        if ((status & STATUS_KEXP_DONE_BIT) != 0) {
            return true;
        }
        timeout--;
    }

    return false;
}

bool CypherPUF_DecryptBlock(const uint8_t* ciphertext_16b, uint8_t* plaintext_16b) {
    uint32_t w0 = ((uint32_t)ciphertext_16b[3] << 24) | ((uint32_t)ciphertext_16b[2] << 16) | ((uint32_t)ciphertext_16b[1] << 8) | ciphertext_16b[0];
    uint32_t w1 = ((uint32_t)ciphertext_16b[7] << 24) | ((uint32_t)ciphertext_16b[6] << 16) | ((uint32_t)ciphertext_16b[5] << 8) | ciphertext_16b[4];
    uint32_t w2 = ((uint32_t)ciphertext_16b[11] << 24) | ((uint32_t)ciphertext_16b[10] << 16) | ((uint32_t)ciphertext_16b[9] << 8) | ciphertext_16b[8];
    uint32_t w3 = ((uint32_t)ciphertext_16b[15] << 24) | ((uint32_t)ciphertext_16b[14] << 16) | ((uint32_t)ciphertext_16b[13] << 8) | ciphertext_16b[12];

    Xil_Out32(base_addr + CYPHERPUF_REG_DATA_IN_0, w0);
    Xil_Out32(base_addr + CYPHERPUF_REG_DATA_IN_1, w1);
    Xil_Out32(base_addr + CYPHERPUF_REG_DATA_IN_2, w2);
    Xil_Out32(base_addr + CYPHERPUF_REG_DATA_IN_3, w3);

    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, CTRL_CLEAR_STATUS_BIT);
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, 0x00000000);

    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, CTRL_START_DECRYPT_BIT);
    Xil_Out32(base_addr + CYPHERPUF_REG_CONTROL, 0x00000000);

    uint32_t status = 0;
    uint32_t timeout = 0xFFFFFF;

    while (timeout > 0) {
        status = Xil_In32(base_addr + CYPHERPUF_REG_STATUS);
        if ((status & STATUS_AES_DONE_BIT) != 0) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        return false;
    }

    uint32_t r0 = Xil_In32(base_addr + CYPHERPUF_REG_DATA_OUT_0);
    uint32_t r1 = Xil_In32(base_addr + CYPHERPUF_REG_DATA_OUT_1);
    uint32_t r2 = Xil_In32(base_addr + CYPHERPUF_REG_DATA_OUT_2);
    uint32_t r3 = Xil_In32(base_addr + CYPHERPUF_REG_DATA_OUT_3);

    plaintext_16b[0] = (uint8_t)(r0 & 0xFF);
    plaintext_16b[1] = (uint8_t)((r0 >> 8) & 0xFF);
    plaintext_16b[2] = (uint8_t)((r0 >> 16) & 0xFF);
    plaintext_16b[3] = (uint8_t)((r0 >> 24) & 0xFF);

    plaintext_16b[4] = (uint8_t)(r1 & 0xFF);
    plaintext_16b[5] = (uint8_t)((r1 >> 8) & 0xFF);
    plaintext_16b[6] = (uint8_t)((r1 >> 16) & 0xFF);
    plaintext_16b[7] = (uint8_t)((r1 >> 24) & 0xFF);

    plaintext_16b[8] = (uint8_t)(r2 & 0xFF);
    plaintext_16b[9] = (uint8_t)((r2 >> 8) & 0xFF);
    plaintext_16b[10] = (uint8_t)((r2 >> 16) & 0xFF);
    plaintext_16b[11] = (uint8_t)((r2 >> 24) & 0xFF);

    plaintext_16b[12] = (uint8_t)(r3 & 0xFF);
    plaintext_16b[13] = (uint8_t)((r3 >> 8) & 0xFF);
    plaintext_16b[14] = (uint8_t)((r3 >> 16) & 0xFF);
    plaintext_16b[15] = (uint8_t)((r3 >> 24) & 0xFF);

    return true;
}

void CypherPUF_DecryptBuffer(const uint8_t* ciphertext, uint8_t* plaintext, uint32_t size_bytes) {
    uint32_t blocks = size_bytes / 16;
    for (uint32_t i = 0; i < blocks; i++) {
        CypherPUF_DecryptBlock(&ciphertext[i * 16], &plaintext[i * 16]);
    }
}

void CypherPUF_GetPUFKey(uint8_t* key_buffer_32b) {
    for (int i = 0; i < 8; i++) {
        uint32_t word = Xil_In32(base_addr + CYPHERPUF_REG_PUF_KEY_0 + (i * 4));
        key_buffer_32b[i * 4 + 0] = (uint8_t)(word & 0xFF);
        key_buffer_32b[i * 4 + 1] = (uint8_t)((word >> 8) & 0xFF);
        key_buffer_32b[i * 4 + 2] = (uint8_t)((word >> 16) & 0xFF);
        key_buffer_32b[i * 4 + 3] = (uint8_t)((word >> 24) & 0xFF);
    }
}
