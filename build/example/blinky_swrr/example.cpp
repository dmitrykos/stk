/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk_config.h>
#include <stk.h>
#include "example.h"

static void InitLeds()
{
    Led::Init(Led::RED,   false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE,  false);
}

// Display CPU share by LED on/off times
static void SetLed(uint8_t id)
{
    Led::Set(Led::RED,   id == 0);
    Led::Set(Led::GREEN, id == 1);
    Led::Set(Led::BLUE,  id == 2);
}

// R2350 requires larger stack due to stack-memory heavy SDK API
#ifdef _PICO_H
enum { TASK_STACK_SIZE = 1024 };
#else
enum { TASK_STACK_SIZE = 256 };
#endif

// Generic LED-blink task
template <int32_t _Weight, stk::EAccessMode _AccessMode>
class LedTask : public stk::TaskW<_Weight, TASK_STACK_SIZE, _AccessMode>
{
    uint8_t m_task_id;
public:
    LedTask(uint8_t id) : m_task_id(id)
    {}

    stk::RunFuncType GetFunc() override { return &Run; }
    void *GetFuncUserData() override { return this; }

private:
    static void Run(void *user_data)
    {
        ((LedTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        while (true)
        {
            // toggle LED for this task
            {
                // protect from preemption during hardware IO
                stk::ScopedCriticalSection __cs;

                SetLed(m_task_id);
            }
        }
    }
};

void RunExample()
{
    using namespace stk;

    InitLeds();

    // 3 tasks kernel with Smooth Weighted Round-Robin scheduler
    static Kernel<KERNEL_STATIC, 3, SwitchStrategySmoothWeightedRoundRobin, PlatformDefault> kernel;

    // assign weights (1000 is a granularity of weights, we split it between all tasks proportionally):
    //  task0: 60% of all time spent by tasks - seen very often (bright, RED)
    //  task1: 30% of all time spent by tasks - seen less (less than half brightness, GREEN)
    //  task2: 10% of all time spent by tasks - seen least (lowest brightness, BLUE)
    static LedTask<(1000 * 60) / 100, ACCESS_PRIVILEGED> task_red(0);
    static LedTask<(1000 * 30) / 100, ACCESS_PRIVILEGED> task_green(1);
    static LedTask<(1000 * 10) / 100, ACCESS_PRIVILEGED> task_blue(2);

    kernel.Initialize();

    kernel.AddTask(&task_red);
    kernel.AddTask(&task_green);
    kernel.AddTask(&task_blue);

    // start scheduling (blocks forever)
    kernel.Start();
    STK_ASSERT(false);
}
