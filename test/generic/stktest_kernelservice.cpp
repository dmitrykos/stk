/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"
using namespace stk;

// ============================================================================ //
// =+========================= KernelService ================================== //
// ============================================================================ //

struct KernelServiceMock : public IKernelService
{
    KernelServiceMock()
    {
        m_inc_ticks  = false;
        m_ticks      = 0;
        m_resolution = 0;
    }
    virtual ~KernelServiceMock()
    { }

    int64_t GetTicks() const
    {
        if (m_inc_ticks)
            const_cast<int64_t &>(m_ticks) = m_ticks + 1;

        return m_ticks;
    }

    int32_t GetTicksResolution() const
    {
        return m_resolution;
    }

    bool    m_inc_ticks;
    int64_t m_ticks;
    int32_t m_resolution;
};

TEST_GROUP(TestKernelService)
{
    void setup() {}
    void teardown() {}
};

TEST(TestKernelService, GetDeadlineTicks)
{
    KernelServiceMock mock;
    mock.m_ticks = 1;

    mock.m_resolution = 1;
    CHECK_EQUAL(10001, (int32_t)mock.GetDeadlineTicks(10));

    mock.m_resolution = 100;
    CHECK_EQUAL(101, (int32_t)mock.GetDeadlineTicks(10));
}

TEST(TestKernelService, DelaySpin)
{
    KernelServiceMock mock;
    mock.m_inc_ticks  = true;
    mock.m_ticks      = 0;
    mock.m_resolution = 1;

    mock.DelaySpin(10);

    CHECK_EQUAL(10001, (int32_t)mock.m_ticks);
}

TEST(TestKernelService, GetTicksResolution)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;
    const uint32_t periodicity = Kernel<2>::PERIODICITY_DEFAULT + 1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);
    kernel.Start(periodicity);

    CHECK_EQUAL(periodicity, g_KernelService->GetTicksResolution());
}

TEST(TestKernelService, GetTicks)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;
    Stack *idle, *active;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start(Kernel<2>::PERIODICITY_DEFAULT);

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick 1-st time
    platform.m_event_handler->OnSysTick(&idle, &active);
    CHECK_EQUAL(1, (int32_t)g_KernelService->GetTicks());

    // ISR calls OnSysTick 2-nd time
    platform.m_event_handler->OnSysTick(&idle, &active);
    CHECK_EQUAL(2, (int32_t)g_KernelService->GetTicks());
}
