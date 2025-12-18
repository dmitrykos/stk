/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdio.h>
#include "example.h"

/* QEMU
   Machine name: STM32F4-Discovery
   CPU name: cortex-m4
*/

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RunExample();

    // should not reach here
    return 1;
}
