/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk_config.h>
#include <stk.h>
#include "example.h"

static volatile uint8_t g_TaskSwitch = 0;

#if 0
static void DelaySpin(uint32_t delay_ms)
{
    // Cortex-M4 instructions: https://developer.arm.com/documentation/ddi0439/b/CHDDIGAC

    // ldr     r3, [sp, #4]
    // subs    r3, #1
    // cmp     r3, #0
    // str     r3, [sp, #4]

    volatile int32_t i = delay_ms * ((SystemCoreClock / 1000) / ((2 + 1 + 1 + 2) * 4));
    while (--i > 0);
}
#else
static __stk_forceinline void DelaySpin(uint32_t delay_ms)
{
    g_KernelService->DelaySpin(delay_ms);
}
#endif

template <stk::EAccessMode _AccessMode>
class MyTask : public stk::Task<256, _AccessMode>
{
    uint8_t m_taskId;

public:
    MyTask(uint8_t taskId) : m_taskId(taskId)
    { }

    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
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
            	LED_SET_STATE(LED_RED, true);
            	LED_SET_STATE(LED_GREEN, false);
            	LED_SET_STATE(LED_BLUE, false);
                break;
            case 1:
            	LED_SET_STATE(LED_RED, false);
            	LED_SET_STATE(LED_GREEN, true);
            	LED_SET_STATE(LED_BLUE, false);
                break;
            case 2:
            	LED_SET_STATE(LED_RED, false);
            	LED_SET_STATE(LED_GREEN, false);
            	LED_SET_STATE(LED_BLUE, true);
                break;
            }

            DelaySpin(1000);

            g_TaskSwitch = task_id + 1;
            if (g_TaskSwitch > 2)
                g_TaskSwitch = 0;
        }
    }
};

static void InitLeds()
{
    LED_INIT(LED_RED, false);
    LED_INIT(LED_GREEN, false);
    LED_INIT(LED_BLUE, false);
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    static Kernel<10> kernel;
    static PlatformArmCortexM platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static MyTask<ACCESS_PRIVILEGED> task1(0);
    static MyTask<ACCESS_USER> task2(1);
    static MyTask<ACCESS_USER> task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(1000);

    assert(false);
    while (true);
}

