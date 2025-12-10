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

// R2350 requires larger stack due to stack-memory heavy SDK API
#ifdef _PICO_H
enum { TASK_STACK_SIZE = 1024 };
#else
enum { TASK_STACK_SIZE = 256 };
#endif

template <stk::EAccessMode _AccessMode>
class MyTask : public stk::Task<TASK_STACK_SIZE, _AccessMode>
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
        for (;;)
        {
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

            stk::Yield();
        }
    }

    void OnDeadlineMissed(uint32_t duration)
    {
        (void)duration;
    }
};

static void InitLeds()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

// optional: you can override sleep and hard fault default behaviors
class PlatformEventHandler : public stk::IPlatform::IEventOverrider
{
private:
    bool OnSleep()
    {
        // if handled return true, otherwise event will be handler by the driver
        // note: if returned false once this function will not be called again until Kernel is re-started
        return false;
    }

    bool OnHardFault()
    {
        // switch on Red LED as indication of the error
        Led::Set(Led::RED, true);
        Led::Set(Led::GREEN, false);
        Led::Set(Led::BLUE, false);

        // if handled return true, otherwise event will be handler by the driver
        // note: prior a call to this function a task which had deadline missed had a call to OnDeadlineMissed
        return false;
    }
};

void RunExample()
{
    using namespace stk;

    InitLeds();

    static Kernel<KERNEL_STATIC | KERNEL_HRT, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;
    static PlatformEventHandler overrider;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static MyTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);

    kernel.Initialize();

    // optional: you can override sleep and hard fault default behaviors
    kernel.GetPlatform()->SetEventOverrider(&overrider);

#define TICKS(MS) GetTicksFromMsec(MS, PERIODICITY_DEFAULT)

    //                     periodicity      deadline    start delay
    kernel.AddTask(&task1, TICKS(1000 * 3), TICKS(100), TICKS(1000 * 0));
    kernel.AddTask(&task2, TICKS(1000 * 3), TICKS(100), TICKS(1000 * 1));
    kernel.AddTask(&task3, TICKS(1000 * 3), TICKS(100), TICKS(1000 * 2));

    kernel.Start();

    // shall not reach here
    STK_ASSERT(false);
}

