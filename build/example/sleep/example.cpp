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
        // task 0: sleep 1000 ms
        // task 1: sleep 2000 ms
        // task 2: sleep 3000 ms
        g_KernelService->Sleep(1000 * (m_task_id + 1));

        switch (m_task_id)
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

    static Kernel<KERNEL_DYNAMIC, 3> kernel;
    static PlatformDefault platform;
    static SwitchStrategyRoundRobin tsstrategy;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static MyTask<ACCESS_USER> task1(0), task2(1), task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start();

    for (int i = 0; i < 3; ++i)
    {
        kernel.AddTask(&task1);
        kernel.AddTask(&task2);
        kernel.AddTask(&task3);

        kernel.Start();
    }

    LED_SET_STATE(LED_RED, true);
    LED_SET_STATE(LED_GREEN, true);
    LED_SET_STATE(LED_BLUE, true);

    while (true) {}
}
