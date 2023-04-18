/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_CONFIG_H_
#define STK_CONFIG_H_

// Risc-V
#define _STK_ARCH_RISC_V

// Minimal stack size depending on the configured architecture (STK default is 32).
#if (__riscv_32e != 1)
    #if (__riscv_flen == 0)
        #define STK_STACK_SIZE_MIN 64
    #else
        #define STK_STACK_SIZE_MIN 128
    #endif
#endif

#ifdef _STK_ARCH_RISC_V
    // Redefine if SysTick handler name is different from SysTick_Handler
    //#define _STK_SYSTICK_HANDLER SysTick_Handler

    // Redefine if PendSv handler name is different from PendSV_Handler
    //#define _STK_PENDSV_HANDLER PendSV_Handler

    // Redefine if SVC handler name is different from SVC_Handler
    //#define _STK_SVC_HANDLER SVC_Handler
#endif

#endif /* STK_CONFIG_H_ */
