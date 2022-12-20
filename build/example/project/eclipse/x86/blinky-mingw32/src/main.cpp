/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <stdio.h>
#include <cstdint>
#include <ctime>
#include "example.h"

static const char *GetLedPin(ELed led)
{
    switch (led)
    {
    case LED_RED: return "RED";
    case LED_GREEN: return "GREEN";
    case LED_BLUE: return "BLUE";
    default:
        assert(false);
        return NULL;
    }
}

void Log(const char *label, ELed led, bool state)
{
    static const time_t g_SecNow = time(NULL);
    time_t now = time(NULL);

    printf("%ds [%s]: %s - %s\n", (int32_t)(now - g_SecNow), label, GetLedPin(led), (state ? "ON" : "OFF"));
}

void LED_INIT(ELed led, bool init_state)
{
    Log("LED_INIT", led, init_state);
}

void LED_SET_STATE(ELed led, bool state)
{
    Log("LED_SET_STATE", led, state);
}

int main(void)
{
    // required to show log in Eclipse IDE Console for 64-bit binary
    setbuf(stdout, NULL);

    RunExample();

    // should not reach here
    return 1;
}
