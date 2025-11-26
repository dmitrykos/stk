/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_CONFIG_H_
#define STK_CONFIG_H_

#include "RP2350.h"

// Undefine if MCU is Arm Cortex-M4
#define _STK_ARCH_ARM_CORTEX_M

#ifdef _STK_ARCH_ARM_CORTEX_M
    // Redefine if SysTick handler name is different from SysTick_Handler
    #define _STK_SYSTICK_HANDLER isr_systick

    // Redefine if PendSv handler name is different from PendSV_Handler
    #define _STK_PENDSV_HANDLER isr_pendsv

    // Redefine if SVC handler name is different from SVC_Handler
    #define _STK_SVC_HANDLER isr_svcall
#endif

#endif /* STK_CONFIG_H_ */
