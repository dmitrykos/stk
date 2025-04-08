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

// note: Replace below includes with includes provided by the vendor of your board.
// note: Rename this file to stk_config.h and place to your project folder which references includes (normally where main.cpp is, see /example).

// Example for NXP K66 MCU:
//#include "system_MK66F18.h"
//#include "MK66F18.h"
//#include "core_cm4.h"

// Undefine for Arm Cortex-M4 MCU
//#define _STK_ARCH_ARM_CORTEX_M

#ifdef _STK_ARCH_ARM_CORTEX_M
    // Redefine if SysTick handler name is different from SysTick_Handler
    //#define _STK_SYSTICK_HANDLER SysTick_Handler

    // Redefine if PendSv handler name is different from PendSV_Handler
    //#define _STK_PENDSV_HANDLER PendSV_Handler

    // Redefine if SVC handler name is different from SVC_Handler
    //#define _STK_SVC_HANDLER SVC_Handler
#endif

// Undefine for x86, Windows
//#define _STK_ARCH_WIN32_M

#endif /* STK_CONFIG_H_ */
