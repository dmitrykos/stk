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

#include "cmsis_device.h"

#define _STK_SYSTICK_HANDLER _STK_SYSTICK_HANDLER_DISABLE
#define _STK_ARCH_ARM_CORTEX_M

#endif /* STK_CONFIG_H_ */
