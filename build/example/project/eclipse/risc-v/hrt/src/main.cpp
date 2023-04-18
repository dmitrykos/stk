/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdio.h>
#include <stk_config.h>
#include <stk.h>

#include "example.h"

/* QEMU
   Machine name: virt
   CPU name: RV32
*/

volatile uint32_t _STK_SYSTEM_CLOCK_VAR = 1000000;

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RunExample();

    // should not reach here
    return 1;
}
