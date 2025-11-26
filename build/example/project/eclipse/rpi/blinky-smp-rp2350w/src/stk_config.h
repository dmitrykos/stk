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

#include <RP2350.h>
#include <pico.h>

// Use ARM Cortex-M33 cores of RP2350
#define _STK_ARCH_ARM_CORTEX_M

// If you wish to use STK on both CPU cores, define _STK_CPU_COUNT
#define _STK_ARCH_CPU_COUNT    (2)
#define _STK_ARCH_GET_CPU_ID() (*(uint32_t *)(SIO_BASE + SIO_CPUID_OFFSET)) // see get_core_num() in pico/platform.h

// Let STK know the names of required ISRs, RPI defines them differently from STM32 or NXP
#define _STK_SYSTICK_HANDLER   isr_systick
#define _STK_PENDSV_HANDLER    isr_pendsv
#define _STK_SVC_HANDLER       isr_svcall

#endif /* STK_CONFIG_H_ */
