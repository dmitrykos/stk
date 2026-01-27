/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk.h>
#include "example.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // activate RISC-V core of RP2350:
    // picotool reboot -c riscv

    RunExample();

    // should not reach here
    return 1;
}
