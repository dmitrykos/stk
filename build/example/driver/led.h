/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef DRIVER_LED_H_
#define DRIVER_LED_H_

#include <stdbool.h>

typedef enum LedId {
    LED_RED,
    LED_GREEN,
    LED_BLUE
}
LedId;

#ifdef __cplusplus

struct Led
{
    using Id = LedId;

    static constexpr Id RED   = LED_RED;
    static constexpr Id GREEN = LED_GREEN;
    static constexpr Id BLUE  = LED_BLUE;

    static void Init(Id led, bool init_state);
    static void Set(Id led, bool state);
};

#else

void Led_Init(LedId led, bool init_state);
void Led_Set(LedId led, bool state);

#endif

#endif /* DRIVER_LED_H_ */
