/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk.h>
#include "example.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RunExample();

    // should not reach here
    return 1;
}
