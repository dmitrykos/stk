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
// ============================== Kernel ====================================== //
// ============================================================================ //

TEST_GROUP(Kernel)
{
    void setup() {}
    void teardown() {}
};

TEST(Kernel, MaxTasks)
{
    const int32_t TASKS = 10;
    Kernel<KERNEL_STATIC, TASKS> kernel;
    const int32_t result = Kernel<KERNEL_STATIC, TASKS>::TASKS_MAX;

    CHECK_EQUAL(TASKS, result);
}

TEST(Kernel, InitFailPlatformNull)
{
    Kernel<KERNEL_STATIC, 10> kernel;
    SwitchStrategyRoundRobin switch_strategy;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(NULL, &switch_strategy);
        CHECK_TEXT(false, "Kernel::Initialize() did not fail with NULL platform");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, InitFailSwitchStrategyNull)
{
    Kernel<KERNEL_STATIC, 10> kernel;
    PlatformTestMock platform;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(&platform, NULL);
        CHECK_TEXT(false, "Kernel::Initialize() did not fail with NULL strategy");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, Init)
{
    Kernel<KERNEL_STATIC, 10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    kernel.Initialize(&platform, &switch_strategy);
}

TEST(Kernel, InitDoubleFail)
{
    Kernel<KERNEL_STATIC, 10> kernel;
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

TEST(Kernel, AddTaskNoInit)
{
    Kernel<KERNEL_STATIC, 10> kernel;
    TaskMock<ACCESS_USER> task;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task);
        CHECK_TEXT(false, "AddTask() did not fail");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, AddTask)
{
    Kernel<KERNEL_STATIC, 10> kernel;
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

TEST(Kernel, AddTaskInitStack)
{
    Kernel<KERNEL_STATIC, 10> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    CHECK_EQUAL(&task, platform.m_user_task_InitStack);
    CHECK_EQUAL((size_t)task.GetStack(), platform.m_stack_InitStack->SP);
}

TEST(Kernel, AddTaskMaxOut)
{
    Kernel<KERNEL_STATIC, 2> kernel;
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

TEST(Kernel, AddSameTask)
{
    Kernel<KERNEL_STATIC, 2> kernel;
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

TEST(Kernel, RemoveTask)
{
    Kernel<KERNEL_DYNAMIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);

    kernel.RemoveTask(&task1);
    CHECK_EQUAL_TEXT(&task2, switch_strategy.GetFirst()->GetUserTask(), "Expecting task2 as first");

    kernel.RemoveTask(&task1);
    CHECK_EQUAL_TEXT(&task2, switch_strategy.GetFirst()->GetUserTask(), "Expecting task2 as first (duplicate task1 removal attempt)");

    kernel.RemoveTask(&task2);
    CHECK_TRUE_TEXT(switch_strategy.GetFirst() == NULL, "Expecting none tasks");
}

TEST(Kernel, RemoveTaskNull)
{
    Kernel<KERNEL_DYNAMIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    kernel.Initialize(&platform, &switch_strategy);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.RemoveTask((ITask *)NULL);
        CHECK_TEXT(false, "expecting to fail with NULL argument");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, RemoveTaskUnsupported)
{
    Kernel<KERNEL_STATIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.RemoveTask(&task);
        CHECK_TEXT(false, "expecting to fail in KERNEL_STATIC mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, StartInvalidPeriodicity)
{
    Kernel<KERNEL_STATIC, 2> kernel;
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
        kernel.Start(PERIODICITY_MAX + 1);
        CHECK_TEXT(false, "expecting to fail with too large periodicity");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, StartNotIntialized)
{
    Kernel<KERNEL_STATIC, 2> kernel;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(PERIODICITY_DEFAULT);
        CHECK_TEXT(false, "expecting to fail when not initialized");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, StartNoTasks)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;

    kernel.Initialize(&platform, &switch_strategy);

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start(PERIODICITY_DEFAULT);
        CHECK_TEXT(false, "expecting to fail without tasks");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, Start)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task;
    const uint32_t periodicity = PERIODICITY_MAX - 1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);

    kernel.Start(periodicity);

    CHECK_TRUE(platform.m_started);
    CHECK_TRUE(platform.m_event_handler != NULL);
    CHECK_TRUE(g_KernelService != NULL);
    CHECK_EQUAL(&task, platform.m_first_task_Start->GetUserTask());
    CHECK_EQUAL(periodicity, platform.GetTickResolution());
}

TEST(Kernel, StartBeginISR)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);
    kernel.Start(PERIODICITY_DEFAULT);

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // expect that first task's access mode is requested by kernel
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform.m_access_mode);
}

TEST(Kernel, ContextSwitchOnSysTickISR)
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

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick 1-st time
    {
        platform.m_event_handler->OnSysTick(&idle, &active);

        CHECK_TRUE(idle != NULL);
        CHECK_TRUE(active != NULL);

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

        CHECK_TRUE(idle != NULL);
        CHECK_TRUE(active != NULL);

        // 2-st task is switched from Active and becomes Idle
        CHECK_EQUAL(idle->SP, (size_t)task2.GetStack());

        // 1-nd task becomes Active
        CHECK_EQUAL(active->SP, (size_t)task1.GetStack());

        // context switch requested
        CHECK_EQUAL(2, platform.m_context_switch_nr);
    }
}

TEST(Kernel, ContextSwitchAccessModeChange)
{
    Kernel<KERNEL_STATIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_PRIVILEGED> task2;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start(PERIODICITY_DEFAULT);

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // 1-st task
    CHECK_EQUAL(ACCESS_USER, platform.m_access_mode);

    // ISR calls OnSysTick
    platform.m_event_handler->OnSysTick(&idle, &active);

    // 2-st task
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform.m_access_mode);
}

TEST(Kernel, AbiCompatibility)
{
    class TaskMockAbiCheck : public Task<32, ACCESS_USER>
    {
    public:
        TaskMockAbiCheck() : m_result(0) {}
        RunFuncType GetFunc() { return forced_cast<stk::RunFuncType>(&TaskMockAbiCheck::Run); }
        void *GetFuncUserData() { return this; }
        uint32_t m_result;
    private:
        void Run() { ++m_result; }
    };

    TaskMockAbiCheck mock;

    mock.GetFunc()(mock.GetFuncUserData());
    mock.GetFunc()(mock.GetFuncUserData());

    CHECK_EQUAL(2, mock.m_result);
}

TEST(Kernel, SingleTask)
{
    Kernel<KERNEL_STATIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task);
    kernel.Start(PERIODICITY_DEFAULT);

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick
    platform.m_event_handler->OnSysTick(&idle, &active);

    // expect that with single task nothing changes
    CHECK_EQUAL((Stack *)NULL, idle);
    CHECK_EQUAL((Stack *)platform.m_stack_InitStack, active);
}

TEST(Kernel, OnTaskExit)
{
    Kernel<KERNEL_DYNAMIC, 2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task1;
    TaskMock<ACCESS_PRIVILEGED> task2;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start(PERIODICITY_DEFAULT);

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform.m_event_handler->OnSysTick(&idle, &active);

    // task2 exited (will schedule its removal)
    platform.m_event_handler->OnTaskExit(active);

    // ISR calls OnSysTick (task2 = idle, task1 = active)
    platform.m_event_handler->OnSysTick(&idle, &active);

    // task1 exited (will schedule its removal)
    platform.m_event_handler->OnTaskExit(active);

    // ISR calls OnSysTick
    platform.m_event_handler->OnSysTick(&idle, &active);

    // expecting privileged mode when scheduler is exiting
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform.m_access_mode);

    // no Idle tasks left
    CHECK_EQUAL((Stack *)NULL, idle);

    // Exit trap stack is provided for a long jumpt to the end of Kernel::Start()
    CHECK_EQUAL(platform.m_exit_trap, active);
}

TEST(Kernel, OnTaskExitUnknownOrNull)
{
    Kernel<KERNEL_DYNAMIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task1;
    Stack unk_stack;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.Start(PERIODICITY_DEFAULT);

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform.m_event_handler->OnSysTick(&idle, &active);

    try
    {
        g_TestContext.ExpectAssert(true);
        platform.m_event_handler->OnTaskExit(&unk_stack);
        CHECK_TEXT(false, "expecting to fail on unknown stack");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }

    try
    {
        g_TestContext.ExpectAssert(true);
        platform.m_event_handler->OnTaskExit(NULL);
        CHECK_TEXT(false, "expecting to fail on NULL");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, OnTaskExitUnsupported)
{
    Kernel<KERNEL_STATIC, 1> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_PRIVILEGED> task1;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);
    kernel.Start(PERIODICITY_DEFAULT);

    Stack *idle = NULL, *active = platform.m_stack_InitStack;

    // ISR calls OnStart
    platform.m_event_handler->OnStart();

    // ISR calls OnSysTick
    platform.m_event_handler->OnSysTick(&idle, &active);

    try
    {
        g_TestContext.ExpectAssert(true);
        platform.m_event_handler->OnTaskExit(active);
        CHECK_TEXT(false, "task exiting unsupported in KERNEL_STATIC mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

