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

static volatile uint8_t g_TaskSwitch = 0;

static void InitLeds()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

// Task function switching the LED
static void SwitchOnLED(uint8_t task_id)
{
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
}

// Task's core (thread)
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
    // thread function provided to scheduler by GetFunc()
    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t task_id = m_task_id;

        // just fake counters to demonstrate that scheduler is saving/restoring context correctly
        // preserving values of floating-point and 64 bit variables
        volatile float count = 0;
        volatile uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                // to avoid hot loop and excessive CPU usage sleep 10ms while waiting for the own turn,
                // if scheduler does not have active threads then it will fall into a sleep mode which is
                // saving the consumed power
                stk::Sleep(10);

                ++count_skip;
                continue;
            }

            ++count;

            SwitchOnLED(task_id);

            // sleep 1s and delegate work to another task switching another LED
            stk::Sleep(1000);
            g_TaskSwitch = (task_id + 1) % 3;
        }
    }
};

void RunExample()
{
    using namespace stk;

    InitLeds();

    // allocate scheduling kernel for 3 threads (tasks) with Round-robin scheduling strategy
    static Kernel<KERNEL_STATIC, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static MyTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);

    // init scheduling kernel
    kernel.Initialize();

    // register threads (tasks)
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    // start scheduler (it will start threads added by AddTask), execution in main() will be blocked on this line
    kernel.Start();

    // shall not reach here after Start() was called
    STK_ASSERT(false);
}

