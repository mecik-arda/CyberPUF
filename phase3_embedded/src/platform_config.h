#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#define CYPHERPUF_BASE_ADDR 0x43C00000

#define XILINX_BAREMETAL_SIM 1

#include <stdint.h>

#if XILINX_BAREMETAL_SIM
    #include <stdio.h>
    #include <stdlib.h>
    extern void Sim_WriteReg(uint32_t addr, uint32_t data);
    extern uint32_t Sim_ReadReg(uint32_t addr);
    #define Xil_Out32(Addr, Data) Sim_WriteReg((Addr), (Data))
    #define Xil_In32(Addr)        Sim_ReadReg((Addr))
#else
    #include "xil_io.h"
#endif

#endif
