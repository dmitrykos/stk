/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
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
    }
};

static stk::Kernel<stk::KERNEL_DYNAMIC, 3, stk::SwitchStrategyRoundRobin, stk::PlatformDefault> g_Kernel;

// note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
static MyTask<stk::ACCESS_PRIVILEGED> g_Task1(0), g_Task2(1), g_Task3(2);

static void InitLeds()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

static void RunCycle()
{
    g_Kernel.AddTask(&g_Task1);
    g_Kernel.AddTask(&g_Task2);
    g_Kernel.AddTask(&g_Task3);

    g_Kernel.Start();
}

void RunExample()
{
    InitLeds();

    g_Kernel.Initialize();

    for (int i = 0; i < 4; ++i)
    {
        RunCycle();
    }

    Led::Set(Led::RED, true);
    Led::Set(Led::GREEN, true);
    Led::Set(Led::BLUE, true);

    while (true) {}
}
