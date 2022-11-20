/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdio.h>
#include "stm32f1xx.h"
#include "stm32f1xx_hal_gpio.h"
#include "example.h"

#define LED_PORT GPIOA

static void InitLedGpio(int32_t pin)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pin = pin;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static uint16_t GetLedPin(ELed led)
{
    switch (led)
    {
    case LED_RED: return GPIO_PIN_MASK; // unavailable on NUCLEO-F103RB board
    case LED_GREEN: return GPIO_PIN_5;
    case LED_BLUE: return GPIO_PIN_MASK; // unavailable on NUCLEO-F103RB board
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
    return 0 ;
}
