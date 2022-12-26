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
using namespace stk::test;

// ============================================================================ //
// =+========================= KernelService ================================== //
// ============================================================================ //

TEST_GROUP(KernelService)
{
    void setup() {}
    void teardown() {}
};

TEST(KernelService, ConvertMicrosecondsToTicks)
{
    static KernelServiceMock mock;
    mock.m_ticks = 1;

    mock.m_resolution = 1;
    CHECK_EQUAL(10000, (int32_t)mock.ConvertMicrosecondsToTicks(10 * 1000));

    mock.m_resolution = 100;
    CHECK_EQUAL(100, (int32_t)mock.ConvertMicrosecondsToTicks(10 * 1000));
}

TEST(KernelService, Delay)
{
    KernelServiceMock mock;
    mock.m_inc_ticks  = true;
    mock.m_ticks      = 0;
    mock.m_resolution = 1;

    mock.Delay(10);

    CHECK_EQUAL(10001, (int32_t)mock.m_ticks);
}

TEST(KernelService, InitStackFailure)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;
    platform.m_fail_InitStack = true;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(&platform, &switch_strategy);
        kernel.AddTask(&task);
        CHECK_TEXT(false, "AddTask() did not fail");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(KernelService, GetTickResolution)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;
    const uint32_t periodicity = PERIODICITY_DEFAULT + 1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);
    kernel.Start(periodicity);

    CHECK_EQUAL(periodicity, g_KernelService->GetTickResolution());
}

TEST(KernelService, GetTicks)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start(PERIODICITY_DEFAULT);

    // ISR calls OnStart
    platform.EventStart();

    // ISR calls OnSysTick 1-st time
    platform.EventSysTick();
    CHECK_EQUAL(1, (int32_t)g_KernelService->GetTicks());

    // ISR calls OnSysTick 2-nd time
    platform.EventSysTick();
    CHECK_EQUAL(2, (int32_t)g_KernelService->GetTicks());
}

static struct SwitchToNextRelaxCpuContext
{
    SwitchToNextRelaxCpuContext()
    {
        counter  = 0;
        platform = NULL;
        task1    = NULL;
        task2    = NULL;
    }

    uint32_t               counter;
    PlatformTestMock      *platform;
    TaskMock<ACCESS_USER> *task1, *task2;

    void Process()
    {
        Stack *&active = platform->m_stack_active;

        platform->EventSysTick();

        // ISR calls OnSysTick (task1 = active, task2 = idle)
        if (counter == 0)
        {
            CHECK_EQUAL(active->SP, (size_t)task1->GetStack());
        }
        else
        // ISR calls OnSysTick (task1 = idle, task2 = active)
        if (counter == 1)
        {
            CHECK_EQUAL(active->SP, (size_t)task2->GetStack());
        }
        // ISR calls OnSysTick (task1 = active, task2 = idle)
        if (counter == 2)
        {
            CHECK_EQUAL(active->SP, (size_t)task1->GetStack());
        }

        ++counter;
    }
}
g_SwitchToNextRelaxCpuContext;

static void SwitchToNextRelaxCpu()
{
    g_SwitchToNextRelaxCpuContext.Process();
}

TEST(KernelService, SwitchToNext)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1, task2;
    Stack *&idle = platform.m_stack_idle, *&active = platform.m_stack_active;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // ISR calls OnStart (task1 = active, task2 = idle)
    platform.EventStart();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform.EventSysTick();
    CHECK_EQUAL(active->SP, (size_t)task2.GetStack());

    g_RelaxCpuHandler = SwitchToNextRelaxCpu;
    g_SwitchToNextRelaxCpuContext.platform = &platform;
    g_SwitchToNextRelaxCpuContext.task1    = &task1;
    g_SwitchToNextRelaxCpuContext.task2    = &task2;

    // task1 calls SwitchToNext (to test path: IKernelService::SwitchToNext -> IPlatform::SwitchToNext -> Kernel::SwitchToNext)
    g_KernelService->SwitchToNext();
    CHECK_EQUAL(1, platform.m_switch_to_next_nr);

    // task2 calls SwitchToNext (due to context switch it became idle task)
    platform.EventTaskSwitch(idle->SP);

    // task1 calls SwitchToNext (task1 = active, task2 = idle)
    platform.EventTaskSwitch(active->SP + 1); // add shift to test IsMemoryOfSP

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform.EventSysTick();
    CHECK_EQUAL(active->SP, (size_t)task2.GetStack());

    g_RelaxCpuHandler = NULL;
}

static struct SleepRelaxCpuContext
{
    SleepRelaxCpuContext()
    {
        counter  = 0;
        platform = NULL;
        task1    = NULL;
        task2    = NULL;
    }

    uint32_t               counter;
    PlatformTestMock      *platform;
    TaskMock<ACCESS_USER> *task1, *task2;

    void Process()
    {
        Stack *&active = platform->m_stack_active;

        platform->EventSysTick();

        // ISR calls OnSysTick (task1 = active, task2 = idle)
        if (counter == 0)
        {
            CHECK_EQUAL(active->SP, (size_t)task1->GetStack());
        }
        else
        // ISR calls OnSysTick (task1 = active, task2 = idle)
        if (counter == 1)
        {
            CHECK_EQUAL(active->SP, (size_t)task1->GetStack());
        }
        else
        // ISR calls OnSysTick (task1 = idle, task2 = active)
        if (counter == 2)
        {
            CHECK_EQUAL(active->SP, (size_t)task2->GetStack());
        }

        ++counter;
    }
}
g_SleepRelaxCpuContext;

static void SleepRelaxCpu()
{
    g_SleepRelaxCpuContext.Process();
}

TEST(KernelService, Sleep)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1, task2;
    Stack *&active = platform.m_stack_active;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // ISR calls OnStart (task1 = active, task2 = idle)
    platform.EventStart();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform.EventSysTick();
    CHECK_EQUAL(active->SP, (size_t)task2.GetStack());

    g_RelaxCpuHandler = SleepRelaxCpu;
    g_SleepRelaxCpuContext.platform = &platform;
    g_SleepRelaxCpuContext.task1    = &task1;
    g_SleepRelaxCpuContext.task2    = &task2;

    // task1 calls Sleep (task1 = active, task2 = idle)
    g_KernelService->Sleep(2);

    // ISR calls OnSysTick (task1 = active, task2 = idle)
    platform.EventSysTick();
    CHECK_EQUAL(active->SP, (size_t)task1.GetStack());

    g_RelaxCpuHandler = NULL;
}

static struct SleepAllAndWakeRelaxCpuContext
{
    SleepAllAndWakeRelaxCpuContext()
    {
        counter  = 0;
        platform = NULL;
        task1    = NULL;
    }

    uint32_t               counter;
    PlatformTestMock      *platform;
    TaskMock<ACCESS_USER> *task1;

    void Process()
    {
        Stack *&idle = platform->m_stack_idle, *&active = platform->m_stack_active;

        platform->EventSysTick();

        // ISR calls OnSysTick (task1 = idle, sleep_trap = active)
        if (counter == 0)
        {
            CHECK_EQUAL(idle->SP, (size_t)task1->GetStack());
            CHECK_EQUAL(active->SP, (size_t)platform->m_stack_info[STACK_SLEEP_TRAP].stack->SP);
        }
        else
        if (counter == 1)
        {
            // to check FSM_STATE_NONE case
        }
        else
        // ISR calls OnSysTick (task1 = active, sleep_trap = idle)
        if (counter == 2)
        {
            CHECK_EQUAL(active->SP, (size_t)task1->GetStack());
            CHECK_EQUAL(idle->SP, (size_t)platform->m_stack_info[STACK_SLEEP_TRAP].stack->SP);
        }

        ++counter;
    }
}
g_SleepAllAndWakeRelaxCpuContext;

static void SleepAllAndWakeRelaxCpu()
{
    g_SleepAllAndWakeRelaxCpuContext.Process();
}

TEST(KernelService, SleepAllAndWake)
{
    Kernel<KERNEL_STATIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.Start();

    // ISR calls OnStart (task1 = active)
    platform.EventStart();

    g_RelaxCpuHandler = SleepAllAndWakeRelaxCpu;
    g_SleepAllAndWakeRelaxCpuContext.platform = &platform;
    g_SleepAllAndWakeRelaxCpuContext.task1    = &task1;

    // task1 calls Sleep
    g_KernelService->Sleep(3);

    g_RelaxCpuHandler = NULL;
}
