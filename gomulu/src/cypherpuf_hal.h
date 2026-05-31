#ifndef CYPHERPUF_HAL_H
#define CYPHERPUF_HAL_H

#include <stdint.h>
#include <stdbool.h>

#define CYPHERPUF_REG_CONTROL      0x00
#define CYPHERPUF_REG_STATUS       0x04
#define CYPHERPUF_REG_DATA_IN_0    0x08
#define CYPHERPUF_REG_DATA_IN_1    0x0C
#define CYPHERPUF_REG_DATA_IN_2    0x10
#define CYPHERPUF_REG_DATA_IN_3    0x14
#define CYPHERPUF_REG_DATA_OUT_0   0x18
#define CYPHERPUF_REG_DATA_OUT_1   0x1C
#define CYPHERPUF_REG_DATA_OUT_2   0x20
#define CYPHERPUF_REG_DATA_OUT_3   0x24
#define CYPHERPUF_REG_PUF_KEY_0    0x28
#define CYPHERPUF_REG_DEBUG_0      0x48
#define CYPHERPUF_REG_DEBUG_1      0x4C

#define CTRL_GENERATE_KEY_BIT      (1 << 0)
#define CTRL_START_DECRYPT_BIT     (1 << 1)
#define CTRL_CLEAR_STATUS_BIT      (1 << 4)

#define STATUS_PUF_BUSY_BIT        (1 << 0)
#define STATUS_PUF_DONE_BIT        (1 << 1)
#define STATUS_KEXP_BUSY_BIT       (1 << 2)
#define STATUS_KEXP_DONE_BIT       (1 << 3)
#define STATUS_AES_BUSY_BIT        (1 << 4)
#define STATUS_AES_DONE_BIT        (1 << 5)

void CypherPUF_Init(uint32_t base_address);

bool CypherPUF_GenerateKey(void);

bool CypherPUF_DecryptBlock(const uint8_t* ciphertext_16b, uint8_t* plaintext_16b);

void CypherPUF_DecryptBuffer(const uint8_t* ciphertext, uint8_t* plaintext, uint32_t size_bytes);

void CypherPUF_GetPUFKey(uint8_t* key_buffer_32b);

uint32_t CypherPUF_GetStatus(void);

#endif
