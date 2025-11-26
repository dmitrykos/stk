/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef DRIVER_SMP_H_
#define DRIVER_SMP_H_

#include <stdint.h>

struct Cpu
{
    static void Start(uint8_t cpu_id, void (*entry_func)(void));
};

#endif /* DRIVER_SMP_H_ */
