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
// ============================== Kernel ====================================== //
// ============================================================================ //

TEST_GROUP(TestKernel)
{
    void setup() {}
    void teardown() {}
};

TEST(TestKernel, MaxTasks)
{
    const int32_t TASKS = 10;
    Kernel<TASKS> kernel;

    CHECK_EQUAL(TASKS, Kernel<TASKS>::TASKS_MAX);
}

TEST(TestKernel, InitFailPlatformNull)
{
    Kernel<10> kernel;
    SwitchStrategyRoundRobin switch_strategy;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(NULL, &switch_strategy);
        CHECK_TEXT(false, "Kernel::Initialize() did not fail with platform = NULL");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, InitFailSwitchStrategyNull)
{
    Kernel<10> kernel;
    PlatformTestMock platform;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(&platform, NULL);
        CHECK_TEXT(false, "Kernel::Initialize() did not fail with switch_strategy = NULL");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, Init)
{
    Kernel<10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    kernel.Initialize(&platform, &switch_strategy);
}

TEST(TestKernel, InitDoubleFail)
{
    Kernel<10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(&platform, &switch_strategy);
        kernel.Initialize(&platform, &switch_strategy);
        CHECK_TEXT(false, "duplicate Kernel::Initialize() did not fail");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, AddTaskNoInit)
{
    Kernel<10> kernel;
    TaskMock<ACCESS_USER> task;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task);
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, AddTask)
{
    Kernel<10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);

    IKernelTask *ktask = switch_strategy.GetFirst();
    CHECK_TRUE_TEXT(ktask == NULL, "Expecting none kernel tasks");

    kernel.AddTask(&task);

    ktask = switch_strategy.GetFirst();
    CHECK_TRUE_TEXT(ktask != NULL, "Expecting one kernel task");

    CHECK_TRUE_TEXT(ktask->GetUserTask() == &task, "Expecting just added user task");
}

TEST(TestKernel, AddTaskInitStack)
{
    Kernel<10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    CHECK_EQUAL(&task, platform.m_user_task_InitStack);
    CHECK_EQUAL((size_t)task.GetStack(), platform.m_stack_InitStack->SP);
}

TEST(TestKernel, AddTaskMaxOut)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;
    TaskMock<ACCESS_USER> task3;

    kernel.Initialize(&platform, &switch_strategy);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task1);
        kernel.AddTask(&task2);
        kernel.AddTask(&task3);
        CHECK_TEXT(false, "expecting to fail adding task because max is 2 but adding 3-rd");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, AddSameTask)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task);
        kernel.AddTask(&task);
        CHECK_TEXT(false, "expecting to fail adding the same task");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, StartInvalidPeriodicity)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(0);
        CHECK_TEXT(false, "expecting to fail with 0 periodicity");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(Kernel<2>::PERIODICITY_MAX + 1);
        CHECK_TEXT(false, "expecting to fail with too large periodicity");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, StartNotIntialized)
{
    Kernel<2> kernel;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(Kernel<2>::PERIODICITY_DEFAULT);
        CHECK_TEXT(false, "expecting to fail when not initialized");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, StartNoTasks)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    kernel.Initialize(&platform, &switch_strategy);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(Kernel<2>::PERIODICITY_DEFAULT);
        CHECK_TEXT(false, "expecting to fail without tasks");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(TestKernel, Start)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;
    const uint32_t periodicity = Kernel<2>::PERIODICITY_MAX - 1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    kernel.Start(periodicity);

    CHECK_TRUE(platform.m_started);
    CHECK_TRUE(platform.m_event_handler != NULL);
    CHECK_TRUE(g_KernelService != NULL);
    CHECK_EQUAL(&task, platform.m_first_task_Start->GetUserTask());
    CHECK_EQUAL(periodicity, platform.GetTickResolution());
}

TEST(TestKernel, StartBeginISR)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);
    kernel.Start(Kernel<2>::PERIODICITY_DEFAULT);

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // expect that first task's access mode is requested by kernel
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform.m_access_mode);
}

TEST(TestKernel, ContextSwitchOnSysTickISR)
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
    {
        platform.m_event_handler->OnSysTick(&idle, &active);

        // 1-st task is switched from Active and becomes Idle
        CHECK_EQUAL(idle->SP, (size_t)task1.GetStack());

        // 2-nd task becomes Active
        CHECK_EQUAL(active->SP, (size_t)task2.GetStack());

        // context switch requested
        CHECK_EQUAL(1, platform.m_context_switch_nr);
    }

    // ISR calls OnSysTick 2-nd time
    {
        platform.m_event_handler->OnSysTick(&idle, &active);

        // 2-st task is switched from Active and becomes Idle
        CHECK_EQUAL(idle->SP, (size_t)task2.GetStack());

        // 1-nd task becomes Active
        CHECK_EQUAL(active->SP, (size_t)task1.GetStack());

        // context switch requested
        CHECK_EQUAL(2, platform.m_context_switch_nr);
    }
}

TEST(TestKernel, ContextSwitchAccessModeChange)
{
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_PRIVILEGED> task2;
    Stack *idle, *active;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start(Kernel<2>::PERIODICITY_DEFAULT);

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // 1-st task
    CHECK_EQUAL(ACCESS_USER, platform.m_access_mode);

    // ISR calls OnSysTick
    platform.m_event_handler->OnSysTick(&idle, &active);

    // 2-st task
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform.m_access_mode);
}
