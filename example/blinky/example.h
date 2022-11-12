/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef EXAMPLE_H_
#define EXAMPLE_H_

#include <assert.h>

enum ELed
{
	LED_RED, LED_GREEN, LED_BLUE
};

void LED_INIT(ELed led, bool init_state);
void LED_SET_STATE(ELed led, bool state);

extern void RunExample();

#endif /* EXAMPLE_H_ */
