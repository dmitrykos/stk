/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "../led.h"

static const char *Led_GetPin(Led::Id led)
{
    switch (led)
    {
    case Led::RED: return "RED";
    case Led::GREEN: return "GREEN";
    case Led::BLUE: return "BLUE";
    default:
        assert(false);
        return NULL;
    }
}

void Log(const char *label, Led::Id led, bool state)
{
    static const time_t g_SecNow = time(NULL);
    time_t now = time(NULL);

    printf("%ds [%s]: %s - %s\n", (int32_t)(now - g_SecNow), label, Led_GetPin(led), (state ? "ON" : "OFF"));
}

void Led::Init(Id led, bool init_state)
{
    Log("LED_INIT", led, init_state);

    // required to show log in Eclipse IDE Console for 64-bit binary
#ifdef _WIN32
    setbuf(stdout, NULL);
#endif
}

void Led::Set(Id led, bool state)
{
    Log("LED_SET_STATE", led, state);
}

