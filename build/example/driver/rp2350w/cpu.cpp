/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <assert.h>
#include "pico/stdlib.h"

// Use GPIO of WiFi chip
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#include "pico/multicore.h"

#include "../cpu.h"

void Cpu::Start(uint8_t cpu_id, void (*entry_func)(void))
{
    assert(cpu_id < 2);

    if (cpu_id == 0)
        entry_func();
    else
    if (cpu_id == 1)
        multicore_launch_core1(entry_func);
}
