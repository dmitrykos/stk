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

static uint8_t g_Led = 0;

template <stk::EAccessMode _AccessMode>
class HwLedTask : public stk::Task<TASK_STACK_SIZE, _AccessMode>
{
    uint8_t m_task_id;

public:
    HwLedTask(uint8_t task_id) : m_task_id(task_id)
    {}

#if 0
    stk::RunFuncType GetFunc() { return stk::forced_cast<stk::RunFuncType>(&HwLedTask::RunInner); }
#else
    stk::RunFuncType GetFunc() { return &Run; }
#endif
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((HwLedTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        for (;;)
        {
            if (g_Led == m_task_id)
            {
                g_Led = ~0;

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

            stk::Yield();
        }
    }

    void OnDeadlineMissed(uint32_t duration)
    {
        (void)duration;
    }
};

template <stk::EAccessMode _AccessMode>
class CtrlTask : public stk::Task<TASK_STACK_SIZE, _AccessMode>
{
public:
    CtrlTask()
    {}

    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((CtrlTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t led = 0;
        stk::PeriodicTimer<1000> timer;

        for (;;)
        {
            timer.Update([&led](int64_t /*now*/, int64_t /*period*/) {
                led = (led + 1) % 3;
                g_Led = led;
            });

            stk::Yield();
        }
    }

    void OnDeadlineMissed(uint32_t duration)
    {
        // actual duration of missed task
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
        // if handled inside this function then return true, otherwise event will be handled by the driver
        // note: if returned false once this function will not be called again until Kernel is re-started
        return false;
    }

    bool OnHardFault()
    {
        // switch on Red LED as indication of the error
        Led::Set(Led::RED, true);
        Led::Set(Led::GREEN, false);
        Led::Set(Led::BLUE, false);

        // if handled inside this function then return true, otherwise event will be handled by the driver
        // note: prior a call to this function a task which had deadline missed had a call to OnDeadlineMissed
        return false;
    }
};

void RunExample()
{
    using namespace stk;

    InitLeds();

    enum { TASK_COUNT = 4 };

    static Kernel<KERNEL_STATIC | KERNEL_HRT, TASK_COUNT, SwitchStrategyRM, PlatformDefault> kernel;
    static PlatformEventHandler overrider;

    // assume that hardware LED tasks have highest priority
    static HwLedTask<ACCESS_PRIVILEGED> hwt0(0), hwt1(1), hwt2(2);

    // control task is sending commands to hardware tasks
    static CtrlTask<ACCESS_USER> ctrl;

    kernel.Initialize();

    // optional: you can override sleep and hard fault default behaviors
    kernel.GetPlatform()->SetEventOverrider(&overrider);

#define MSEC(MS) GetTicksFromMsec(MS, PERIODICITY_DEFAULT)

    //                     periodicity      deadline    start delay
    kernel.AddTask(&ctrl, MSEC(100), MSEC(200), MSEC(0));
    kernel.AddTask(&hwt0, MSEC(10), MSEC(100), MSEC(0));
    kernel.AddTask(&hwt1, MSEC(10), MSEC(100), MSEC(0));
    kernel.AddTask(&hwt2, MSEC(10), MSEC(100), MSEC(0));

    auto wcrt_sched = static_cast<SwitchStrategyRM *>(kernel.GetSwitchStrategy())->IsSchedulableWCRT<TASK_COUNT>();
    STK_ASSERT(wcrt_sched == true);

    kernel.Start();

    // shall not reach here
    STK_ASSERT(false);
}

