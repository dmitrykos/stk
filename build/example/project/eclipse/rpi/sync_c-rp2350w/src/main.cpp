/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "example.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // activate ARM Cortex-M core of RP2350:
    // picotool reboot -c arm

    RunExample();
    return 0 ;
}
