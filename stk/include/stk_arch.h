/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_H_
#define STK_ARCH_H_

/*! \file  stk_arch.h
    \brief Contains platform implementations (IPlatform).
*/

#ifdef _STK_ARCH_ARM_CORTEX_M
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"
#endif
#ifdef _STK_ARCH_RISC_V
#include "arch/risc-v/stk_arch_risc-v.h"
#endif
#ifdef _STK_ARCH_X86_WIN32
#include "arch/x86/win32/stk_arch_x86-win32.h"
#endif

#endif /* STK_ARCH_H_ */
