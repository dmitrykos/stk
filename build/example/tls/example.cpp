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

// It is a simple TLS, you can host a high-level implementation instead with access by index
struct MyTls
{
    uint8_t task_id;
};

// Task function switching the LED, TLS provides the ID of the task for the logic of this function
static void ProcessTask()
{
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
    MyTls m_tls; //!< Task-local TLS

    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        // set your TLS (it can host any complex implementation of your choice)
        stk::SetTlsPtr(&m_tls);

        volatile float count = 0;
        volatile uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != _TaskId)
            {
                ++count_skip;
                continue;
            }

            ++count;

            ProcessTask();

            Delay(1000);
            g_TaskSwitch = (_TaskId + 1) % 3;
        }
    }
};

static void InitLeds()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    static Kernel<KERNEL_STATIC, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static MyTask<0, ACCESS_PRIVILEGED> task1;
    static MyTask<1, ACCESS_PRIVILEGED> task2;
    static MyTask<2, ACCESS_PRIVILEGED> task3;

    kernel.Initialize();

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start();

    // shall not reach here
    STK_ASSERT(false);
}
