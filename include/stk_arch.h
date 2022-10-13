/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_H_
#define STK_ARCH_H_

#ifdef _STK_ARCH_ARM_CORTEX_M
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"
#endif

#endif /* STK_ARCH_H_ */
