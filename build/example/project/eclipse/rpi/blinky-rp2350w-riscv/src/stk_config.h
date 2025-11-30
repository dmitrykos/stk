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

#include <stdint.h>
#include <risc-v/encoding.h>
#include <pico.h>

// Risc-V
#define _STK_ARCH_RISC_V

// If you wish to use STK on both CPU cores, define _STK_CPU_COUNT
#define _STK_ARCH_CPU_COUNT    (2)
#define _STK_ARCH_GET_CPU_ID() (*(uint32_t *)(SIO_BASE + SIO_CPUID_OFFSET)) // see get_core_num() in pico/platform.h

#define STK_RISCV_CLINT_MTIME_ADDR        ((volatile uint64_t *)(SIO_BASE + 0x1b0)) // MTIME + MTIMEH
#define STK_RISCV_CLINT_MTIMECMP_ADDR     ((volatile uint64_t *)(SIO_BASE + 0x1b8)) // MTIMECMP + MTIMECMPH
#define STK_RISCV_CLINT_MTIMECMP_PER_HART 0

#define _STK_SYSTICK_HANDLER isr_riscv_machine_timer
#define _STK_SVC_HANDLER     isr_riscv_machine_exception

#define STK_RISCV_ISR_SECTION __attribute__((section(".time_critical." "stk")))

#endif /* STK_CONFIG_H_ */
