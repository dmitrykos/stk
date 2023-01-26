/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <assert.h>
#include "board.h"
#include "../led.h"

void Led::Init(Id led, bool init_state)
{
    uint8_t logic_state = (init_state ? LOGIC_LED_ON : LOGIC_LED_OFF);

    switch (led)
    {
    case Led::RED: break; // unavailabe on board
    case Led::GREEN: USER_LED_INIT(logic_state); break;
    case Led::BLUE: break; // unavailabe on board
    default:
        assert(false);
        break;
    }
}

void Led::Set(Id led, bool state)
{
    switch (led)
    {
    case Led::RED: break; // unavailabe on board
    case Led::GREEN: (state ? USER_LED_ON() : USER_LED_OFF()); break;
    case Led::BLUE: break; // unavailabe on board
    default:
        assert(false);
        break;
    }
}
