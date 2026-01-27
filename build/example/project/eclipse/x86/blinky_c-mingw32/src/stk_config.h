/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_CONFIG_H_
#define STK_CONFIG_H_

#define _STK_ARCH_X86_WIN32

// For C interface:
#define STK_C_CPU_COUNT         (1)
#define STK_C_KERNEL_MAX_TASKS  (3)
#define STK_C_KERNEL_TYPE_CPU_0 Kernel<KERNEL_STATIC | KERNEL_SYNC, STK_C_KERNEL_MAX_TASKS, SwitchStrategyRR, PlatformDefault>

#endif /* STK_CONFIG_H_ */
