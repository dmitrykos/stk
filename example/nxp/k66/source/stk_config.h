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

#include "system_MK66F18.h"
#include "MK66F18.h"
#include "core_cm4.h"

// Undefine if MCU is Arm Cortex-M4
#define _STK_ARCH_ARM_CORTEX_M

#ifdef _STK_ARCH_ARM_CORTEX_M
    // Redefine if SysTick handler name is different from SysTick_Handler
    #define _STK_SYSTICK_HANDLER SysTick_Handler

    // Redefine if PendSv handler name is different from PendSV_Handler
    #define _STK_PENDSV_HANDLER PendSV_Handler
#endif

#endif /* STK_CONFIG_H_ */
