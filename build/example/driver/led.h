/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef DRIVER_LED_H_
#define DRIVER_LED_H_

struct Led
{
    enum Id
    {
        RED,
        GREEN,
        BLUE
    };

    static void Init(Id led, bool init_state);
    static void Set(Id led, bool state);
};

#endif /* DRIVER_LED_H_ */
