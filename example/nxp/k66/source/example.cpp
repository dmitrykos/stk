/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk.h>
#include "board.h"

static volatile uint8_t g_TaskSwitch = 0;

/*#if 0
static void Delay(uint32_t delay_ms)
{
    // Cortex-M4 instructions: https://developer.arm.com/documentation/ddi0439/b/CHDDIGAC

    volatile int32_t i = delay_ms * ((SystemCoreClock / 1000) / ((2 + 1 + 2 + 1) * 8));
    while (--i > 0);
}
#else
static void Delay(uint32_t delay_msec)
{
    int64_t deadline_msec = stk::g_Kernel->GetDeadlineTicks(delay_msec);
    while (stk::g_Kernel->GetTicks() < deadline_msec);
}
#endif*/

template <stk::EAccessMode _AccessMode>
class Task : public stk::UserTask<256, _AccessMode>
{
    uint8_t m_taskId;

public:
    Task(uint8_t taskId) : m_taskId(taskId)
    { }

    stk::RunFuncT GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((Task *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t task_id = m_taskId;

        float count = 0;
        uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                ++count_skip;
                continue;
            }

            ++count;

            switch (task_id)
            {
            case 0:
                LED_RED_ON();
                LED_GREEN_OFF();
                LED_BLUE_OFF();
                break;
            case 1:
                LED_RED_OFF();
                LED_GREEN_ON();
                LED_BLUE_OFF();
                break;
            case 2:
                LED_RED_OFF();
                LED_GREEN_OFF();
                LED_BLUE_ON();
                break;
            }

            stk::g_Kernel->Delay(1000);

            g_TaskSwitch = task_id + 1;
            if (g_TaskSwitch > 2)
                g_TaskSwitch = 0;
        }
    }
};

static void InitLeds()
{
    LED_RED_INIT(LOGIC_LED_OFF);
    LED_GREEN_INIT(LOGIC_LED_OFF);
    LED_BLUE_INIT(LOGIC_LED_OFF);
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    static Kernel<10> kernel;
    static PlatformArmCortexM platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static Task<ACCESS_PRIVILEGED> task1(0);
    static Task<ACCESS_USER> task2(1);
    static Task<ACCESS_USER> task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(1000);

    assert(false);
    while (true);
}
