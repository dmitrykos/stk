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
static void Delay(uint32_t delay_ms)
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
static __stk_forceinline void Delay(uint32_t delay_ms)
{
    g_KernelService->Delay(delay_ms);
}
#endif

template <stk::EAccessMode _AccessMode>
class MyTask : public stk::Task<256, _AccessMode>
{
    uint8_t m_task_id;

public:
    MyTask(uint8_t task_id) : m_task_id(task_id)
    { }

#if 0
    stk::RunFuncType GetFunc() { return stk::forced_cast<stk::RunFuncType>(&MyTask::RunInner); }
#else
    stk::RunFuncType GetFunc() { return &Run; }
#endif
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t task_id = m_task_id;

        volatile float count = 0;
        volatile uint64_t count_skip = 0;

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
                Led::Set(Led::RED, true);
                Led::Set(Led::GREEN, false);
                Led::Set(Led::BLUE, false);
                break;
            case 1:
                Led::Set(Led::RED, false);
                Led::Set(Led::GREEN, true);
                Led::Set(Led::BLUE, false);
                break;
            case 2:
                Led::Set(Led::RED, false);
                Led::Set(Led::GREEN, false);
                Led::Set(Led::BLUE, true);
                break;
            }

            Delay(1000);

            g_TaskSwitch = (task_id + 1) % 3;
            return;
        }
    }
};

static void InitLeds()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

static stk::Kernel<stk::KERNEL_DYNAMIC, 3, stk::SwitchStrategyRoundRobin, stk::PlatformDefault> g_Kernel;

// note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
static MyTask<stk::ACCESS_PRIVILEGED> g_Task1(0), g_Task2(1), g_Task3(2);

static void RunOnce()
{
    g_Kernel.AddTask(&g_Task1);
    g_Kernel.AddTask(&g_Task2);
    g_Kernel.AddTask(&g_Task3);

    g_TaskSwitch = 0;

    // note: kernel will exit from Start() once all tasks exit (complete their work)
    g_Kernel.Start();
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    g_Kernel.Initialize();

    for (int i = 0; i < 3; ++i)
    {
        RunOnce();
    }

    Led::Set(Led::RED, true);
    Led::Set(Led::GREEN, true);
    Led::Set(Led::BLUE, true);

    while (true) {}
}
