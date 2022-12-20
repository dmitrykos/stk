/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"
#include "example.h"

#define LED_PORT GPIOD

static void InitLedGpio(int32_t pin)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pin = pin;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = 0;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static int32_t GetLedPin(ELed led)
{
    switch (led)
    {
    case LED_RED: return GPIO_PIN_14;
    case LED_GREEN: return GPIO_PIN_12;
    case LED_BLUE: return GPIO_PIN_15;
    default:
        assert(false);
        return GPIO_PIN_MASK;
    }
}

void LED_INIT(ELed led, bool init_state)
{
    uint16_t pin = GetLedPin(led);
    if (GPIO_PIN_MASK != pin)
    {
        InitLedGpio(GetLedPin(led));
        LED_SET_STATE(led, init_state);
    }
}

void LED_SET_STATE(ELed led, bool state)
{
    uint16_t pin = GetLedPin(led);
    if (GPIO_PIN_MASK != pin)
        HAL_GPIO_WritePin(LED_PORT, pin, (state ? GPIO_PIN_SET : GPIO_PIN_RESET));
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RunExample();

    // should not reach here
    return 1;
}
