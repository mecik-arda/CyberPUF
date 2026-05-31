#ifndef PLATFORM_YAPILANDIRMASI_H
#define PLATFORM_YAPILANDIRMASI_H

#define CYPHERPUF_TABAN_ADRES 0x43C00000

#define XILINX_BAREMETAL_SIM 1

#include <stdint.h>

#if XILINX_BAREMETAL_SIM
    #include <stdio.h>
    #include <stdlib.h>
    extern void Sim_RegYaz(uint32_t adres, uint32_t data);
    extern uint32_t Sim_RegOku(uint32_t adres);
    #define Xil_Out32(Addr, Data) Sim_RegYaz((Addr), (Data))
    #define Xil_In32(Addr)        Sim_RegOku((Addr))
#else
    #include "xil_io.h"
#endif

#endif
