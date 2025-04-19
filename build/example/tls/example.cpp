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

// It is a simple TLS, you can host a high-level implementation instead with access by index
struct MyTls
{
    uint8_t task_id;
};

static void InitLEDs()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

// Task function switching the LED, TLS provides the ID of the task for the logic of this function
static void SwitchOnLED()
{
    // for demonstration purpose we get task_id from our TLS and switch on corresponding LED
    uint8_t task_id = stk::GetTlsPtr<MyTls>()->task_id;

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

// Task's core
template <uint8_t _TaskId, stk::EAccessMode _AccessMode>
class MyTask : public stk::Task<256, _AccessMode>
{
public:
#if 0
    stk::RunFuncType GetFunc() { return stk::forced_cast<stk::RunFuncType>(&MyTask::RunInner); }
#else
    stk::RunFuncType GetFunc() { return &Run; }
#endif
    void *GetFuncUserData() { return this; }

    MyTask()
    {
        m_tls.task_id = _TaskId;
    }

private:
    MyTls m_tls; // task-local TLS, you can provide your own implementation

    // thread function provided to scheduler by GetFunc()
    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        // set your TLS (it can host any complex implementation of your choice)
        stk::SetTlsPtr(&m_tls);

        // just fake counters to demonstrate that scheduler is saving/restoring context correctly
        // preserving values of floating-point and 64 bit variables
        volatile float count = 0;
        volatile uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != _TaskId)
            {
                // to avoid hot loop and excessive CPU usage sleep 10ms while waiting for the own turn,
                // if scheduler does not have active threads then it will fall into a sleep mode which
                // saving the consumed power
                g_KernelService->Sleep(10);

                ++count_skip;
                continue;
            }

            ++count;

            SwitchOnLED();

            // sleep 1s and delegae work to another task switching another LED
            g_KernelService->Sleep(1000);
            g_TaskSwitch = (_TaskId + 1) % 3;
        }
    }
};

void RunExample()
{
    using namespace stk;

    InitLEDs();

    // allocate scheduling kernel for 3 threads (tasks) with Round-robin scheduling strategy
    static Kernel<KERNEL_STATIC, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static MyTask<0, ACCESS_PRIVILEGED> task1;
    static MyTask<1, ACCESS_PRIVILEGED> task2;
    static MyTask<2, ACCESS_PRIVILEGED> task3;

    // init scheduling kernel
    kernel.Initialize();

    // register threads (tasks)
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    // start scheduler (it will start threads added by AddTask), execution in main() will be blocked on this line
    kernel.Start();

    // shall not reach here
    STK_ASSERT(false);
}
