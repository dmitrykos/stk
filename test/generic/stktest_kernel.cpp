/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"

namespace stk {
namespace test {

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
    const int32_t TASKS = 2;
    Kernel<KERNEL_STATIC, TASKS, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    const int32_t result = Kernel<KERNEL_STATIC, TASKS, SwitchStrategyRoundRobin, PlatformTestMock>::TASKS_MAX;

    CHECK_EQUAL(TASKS, result);
}

TEST(Kernel, Init)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    CHECK_TRUE(platform != NULL);

    kernel.Initialize();

    CHECK_TRUE(((IKernel *)&kernel)->IsInitialized());

    CHECK_TRUE(IKernelService::GetInstance() != NULL);
    CHECK_TRUE(IKernelService::GetInstance() == platform->m_service);
}

TEST(Kernel, InitDoubleFail)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize();
        kernel.Initialize();
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
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
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
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    SwitchStrategyRoundRobin *strategy = (SwitchStrategyRoundRobin *)((IKernel &)kernel).GetSwitchStrategy();

    kernel.Initialize();

    CHECK_EQUAL_TEXT(0, strategy->GetSize(), "Expecting none kernel tasks");

    kernel.AddTask(&task);

    CHECK_EQUAL_TEXT(1, strategy->GetSize(), "Expecting 1 kernel task");

    IKernelTask *ktask = strategy->GetFirst();
    CHECK_TRUE_TEXT(ktask != NULL, "Expecting one kernel task");

    CHECK_TRUE_TEXT(ktask->GetUserTask() == &task, "Expecting just added user task");
}

TEST(Kernel, AddTaskInitStack)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task);

    CHECK_EQUAL(&task, platform->m_stack_info[STACK_USER_TASK].task);
    CHECK_EQUAL((size_t)task.GetStack(), platform->m_stack_info[STACK_USER_TASK].stack->SP);
}

TEST(Kernel, AddTaskFailMaxOut)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;

    kernel.Initialize();

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

TEST(Kernel, AddTaskFailSameTask)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();

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

static struct AddTaskWhenStartedRelaxCpuContext
{
    AddTaskWhenStartedRelaxCpuContext()
    {
        counter  = 0;
        platform = NULL;
    }

    uint32_t          counter;
    PlatformTestMock *platform;

    void Process()
    {
        platform->ProcessTick();
        ++counter;
    }
}
g_AddTaskWhenStartedRelaxCpuContext;

static void AddTaskWhenStartedRelaxCpu()
{
    g_AddTaskWhenStartedRelaxCpuContext.Process();
}

TEST(Kernel, AddTaskWhenStarted)
{
    Kernel<KERNEL_DYNAMIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    SwitchStrategyRoundRobin *strategy = (SwitchStrategyRoundRobin *)((IKernel &)kernel).GetSwitchStrategy();

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    CHECK_EQUAL_TEXT(1, strategy->GetFirst()->GetHead()->GetSize(), "expecting Task1 be added at this stage");

    g_AddTaskWhenStartedRelaxCpuContext.platform = (PlatformTestMock *)kernel.GetPlatform();
    g_RelaxCpuHandler = AddTaskWhenStartedRelaxCpu;

    kernel.AddTask(&task2);

    CHECK_EQUAL_TEXT(2, strategy->GetFirst()->GetHead()->GetSize(), "task2 must be added");
    CHECK_EQUAL_TEXT(1, g_AddTaskWhenStartedRelaxCpuContext.counter, "should add new task within 1 cycle");
}

TEST(Kernel, AddTaskFailStaticStarted)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task2);
        CHECK_TEXT(false, "expecting to AddTask to fail when non KERNEL_DYNAMIC");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, AddTaskFailHrtStarted)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;

    kernel.Initialize();
    kernel.AddTask(&task1, 1, 1, 0);
    kernel.Start();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task2, 1, 1, 0);
        CHECK_TEXT(false, "expecting to AddTask to fail when KERNEL_HRT");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, RemoveTask)
{
    Kernel<KERNEL_DYNAMIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    SwitchStrategyRoundRobin *strategy = (SwitchStrategyRoundRobin *)((IKernel &)kernel).GetSwitchStrategy();

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);

    kernel.RemoveTask(&task1);
    CHECK_EQUAL_TEXT(&task2, strategy->GetFirst()->GetUserTask(), "Expecting task2 as first");

    kernel.RemoveTask(&task1);
    CHECK_EQUAL_TEXT(&task2, strategy->GetFirst()->GetUserTask(), "Expecting task2 as first (duplicate task1 removal attempt)");

    kernel.RemoveTask(&task2);
    CHECK_EQUAL_TEXT(0, strategy->GetSize(), "Expecting none tasks");
}

TEST(Kernel, RemoveTaskFailNull)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;

    kernel.Initialize();

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

TEST(Kernel, RemoveTaskFailUnsupported)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();
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

TEST(Kernel, RemoveTaskFailStarted)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();
    kernel.AddTask(&task);
    kernel.Start();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.RemoveTask(&task);
        CHECK_TEXT(false, "expecting to fail when Kernel has started");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, StartInvalidPeriodicity)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Initialize(0);
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
        kernel.Initialize(PERIODICITY_MAX + 1);
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
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start();
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
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;

    kernel.Initialize();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.Start();
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
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    const uint32_t periodicity = PERIODICITY_MAX - 1;

    kernel.Initialize(periodicity);
    kernel.AddTask(&task);

    kernel.Start();

    CHECK_TRUE(platform->m_started);
    CHECK_TRUE(g_KernelService != NULL);
    CHECK_TRUE(platform->m_stack_active != NULL);
    CHECK_EQUAL((size_t)task.GetStack(), platform->m_stack_active->SP);
    CHECK_EQUAL(periodicity, platform->GetTickResolution());
}

TEST(Kernel, StartBeginISR)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task);
    kernel.Start();

    // expect that first task's access mode is requested by kernel
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform->m_stack_active->mode);
}

TEST(Kernel, ContextSwitchOnSysTickISR)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack *&idle = platform->m_stack_idle, *&active = platform->m_stack_active;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // ISR calls OnSysTick 1-st time
    {
        platform->ProcessTick();

        CHECK_TRUE(idle != NULL);
        CHECK_TRUE(active != NULL);

        // 1-st task is switched from Active and becomes Idle
        CHECK_EQUAL(idle->SP, (size_t)task1.GetStack());

        // 2-nd task becomes Active
        CHECK_EQUAL(active->SP, (size_t)task2.GetStack());

        // context switch requested
        CHECK_EQUAL(1, platform->m_context_switch_nr);
    }

    // ISR calls OnSysTick 2-nd time
    {
        platform->ProcessTick();

        CHECK_TRUE(idle != NULL);
        CHECK_TRUE(active != NULL);

        // 2-st task is switched from Active and becomes Idle
        CHECK_EQUAL(idle->SP, (size_t)task2.GetStack());

        // 1-nd task becomes Active
        CHECK_EQUAL(active->SP, (size_t)task1.GetStack());

        // context switch requested
        CHECK_EQUAL(2, platform->m_context_switch_nr);
    }
}

TEST(Kernel, ContextSwitchAccessModeChange)
{
    Kernel<KERNEL_STATIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_PRIVILEGED> task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // 1-st task
    CHECK_EQUAL(ACCESS_USER, platform->m_stack_active->mode);

    // ISR calls OnSysTick
    platform->ProcessTick();

    // 2-st task
    CHECK_EQUAL(ACCESS_PRIVILEGED, platform->m_stack_active->mode);
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
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack *&idle = platform->m_stack_idle, *&active = platform->m_stack_active;

    kernel.Initialize();
    kernel.AddTask(&task);
    kernel.Start();

    // ISR calls OnSysTick
    platform->ProcessTick();

    // expect that with single task nothing changes
    CHECK_EQUAL((Stack *)NULL, idle);
    CHECK_EQUAL((Stack *)platform->m_stack_info[STACK_USER_TASK].stack, active);
}

TEST(Kernel, OnTaskExit)
{
    Kernel<KERNEL_DYNAMIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack *&idle = platform->m_stack_idle, *&active = platform->m_stack_active;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform->ProcessTick();

    // task2 exited (will schedule its removal)
    platform->EventTaskExit(active);

    // ISR calls OnSysTick (task2 = idle, task1 = active)
    platform->ProcessTick();

    // task1 exited (will schedule its removal)
    platform->EventTaskExit(active);

    // ISR calls OnSysTick
    platform->ProcessTick();

    // no Idle tasks left
    CHECK_EQUAL((Stack *)NULL, idle);

    // Exit trap stack is provided for a long jumpt to the end of Kernel::Start()
    CHECK_EQUAL(platform->m_exit_trap, active);
}

TEST(Kernel, OnTaskExitUnknownOrNull)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task1;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack unk_stack;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    // ISR calls OnSysTick (task1 = idle, task2 = active)
    platform->ProcessTick();

    try
    {
        g_TestContext.ExpectAssert(true);
        platform->EventTaskExit(&unk_stack);
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
        platform->EventTaskExit(NULL);
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
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task1;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack *&active = platform->m_stack_active;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    // ISR calls OnSysTick
    platform->ProcessTick();

    try
    {
        g_TestContext.ExpectAssert(true);
        platform->EventTaskExit(active);
        CHECK_TEXT(false, "task exiting unsupported in KERNEL_STATIC mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, OnTaskNotFoundBySP)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task1;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    platform->ProcessTick();

    try
    {
        g_TestContext.ExpectAssert(true);
        platform->EventTaskSwitch(0xdeadbeef);
        CHECK_TEXT(false, "non existent task must not succeed");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, OnTaskSkipFreedTask)
{
    Kernel<KERNEL_DYNAMIC, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_PRIVILEGED> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    Stack *&active = platform->m_stack_active;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.Start();

    // task1 exited (will schedule its removal)
    platform->EventTaskExit(active);

    // 2 ticks to remove exited task1 from scheduling (1st switched to task2, 2nd cleans up task1 exit)
    platform->ProcessTick();
    platform->ProcessTick();

    try
    {
        g_TestContext.ExpectAssert(true);

        // we loop through all tasks in attempt to find non existent SP (0xdeadbeef)
        // by this FindTaskBySP() is invoked and will loop thorugh the exited task1's
        // slot
        platform->EventTaskSwitch(0xdeadbeef);
        CHECK_TEXT(false, "exited task must be successfully skipped by FindTaskBySP()");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, Hrt)
{
    Kernel<KERNEL_STATIC | KERNEL_HRT, 2, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task1, 1, 1, 0);
    kernel.AddTask(&task2, 2, 1, 0);
    kernel.Start();

    platform->ProcessTick();
    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task2.GetStack());

    platform->ProcessTick();
    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task1.GetStack());
}

TEST(Kernel, HrtAddNonHrt)
{
    Kernel<KERNEL_STATIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task);
        CHECK_TEXT(false, "non-HRT AddTask not supported in HRT mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, HrtAddNotAllowedForNonHrtMode)
{
    Kernel<KERNEL_STATIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();

    try
    {
        g_TestContext.ExpectAssert(true);
        kernel.AddTask(&task, 1, 1, 0);
        CHECK_TEXT(false, "HRT-related AddTask not supported in non-HRT mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, HrtSleepNotAllowed)
{
    Kernel<KERNEL_STATIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;

    kernel.Initialize();
    kernel.AddTask(&task, 1, 1, 0);
    kernel.Start();

    try
    {
        g_TestContext.ExpectAssert(true);
        Sleep(1);
        CHECK_TEXT(false, "IKernelService::Sleep not allowed in HRT mode");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(Kernel, HrtTaskCompleted)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();
    SwitchStrategyRoundRobin *strategy = (SwitchStrategyRoundRobin *)((IKernel &)kernel).GetSwitchStrategy();

    kernel.Initialize();
    kernel.AddTask(&task, 1, 1, 0);
    kernel.Start();

    CHECK_TRUE(strategy->GetSize() != 0);

    platform->EventTaskExit(platform->m_stack_active);
    platform->ProcessTick();

    CHECK_EQUAL(0, strategy->GetSize());
}

static struct HrtTaskDeadlineMissedRelaxCpuContext
{
    HrtTaskDeadlineMissedRelaxCpuContext()
    {
        counter  = 0;
        platform = NULL;
    }

    uint32_t          counter;
    PlatformTestMock *platform;

    void Process()
    {
        platform->ProcessTick();
        ++counter;
    }
}
g_HrtTaskDeadlineMissedRelaxCpuContext;

static void HrtTaskDeadlineMissedRelaxCpu()
{
    g_HrtTaskDeadlineMissedRelaxCpuContext.Process();
}

TEST(Kernel, HrtTaskDeadlineMissed)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task, 2, 1, 0);
    kernel.Start();

    g_HrtTaskDeadlineMissedRelaxCpuContext.platform = platform;
    g_RelaxCpuHandler = HrtTaskDeadlineMissedRelaxCpu;

    platform->ProcessTick();

    g_TestContext.ExpectAssert(true);

    // 2-nd tick goes outside the deadline
    platform->ProcessTick();

    // task completes its work and yields to kernel, its workload is 2 ticks now that is outside deadline 1
    Yield();

    // 3-nd tick goes outside the deadline
    platform->ProcessTick();

    CHECK_TRUE(platform->m_hard_fault);
    CHECK_EQUAL(2, task.m_deadline_missed);
}

TEST(Kernel, HrtTaskDeadlineNotMissed)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task, 2, 1, 0);
    kernel.Start();

    g_HrtTaskDeadlineMissedRelaxCpuContext.platform = platform;
    g_RelaxCpuHandler = HrtTaskDeadlineMissedRelaxCpu;

    platform->ProcessTick();

    // task completes its work and yields to kernel, its workload is 1 ticks now that is within deadline 1
    Yield();

    // 2-nd tick continues scheduling normally
    platform->ProcessTick();

    CHECK_FALSE(platform->m_hard_fault);
    CHECK_EQUAL(0, task.m_deadline_missed);
}

TEST(Kernel, HrtSkipSleepingNext)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 2, SwitchStrategyRM, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task1, 2, 2, 0);
    kernel.AddTask(&task2, 3, 3, 0);
    kernel.Start();

    g_HrtTaskDeadlineMissedRelaxCpuContext.platform = platform;
    g_RelaxCpuHandler = HrtTaskDeadlineMissedRelaxCpu;

    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task1.GetStack());
    platform->ProcessTick();
    platform->ProcessTick();
    Yield();
    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task2.GetStack());
    platform->ProcessTick();
    platform->ProcessTick();
    platform->ProcessTick();
    Yield();
    platform->ProcessTick();
    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task1.GetStack());

    CHECK_FALSE(platform->m_hard_fault);
    CHECK_EQUAL(0, task1.m_deadline_missed);
    CHECK_EQUAL(0, task2.m_deadline_missed);
}

TEST(Kernel, HrtTaskExitDuringSleepState)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 2, SwitchStrategyRM, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task1, 1, 1, 0);
    kernel.AddTask(&task2, 1, 1, 2);
    kernel.Start();

    // task returns (exiting) without calling SwitchToNext
    platform->EventTaskExit(platform->m_stack_active);

    platform->ProcessTick(); // removes first task
    platform->ProcessTick();
}

TEST(Kernel, HrtSleepingAwakeningStateChange)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task;
    PlatformTestMock *platform = (PlatformTestMock *)kernel.GetPlatform();

    kernel.Initialize();
    kernel.AddTask(&task, 1, 1, 1);
    kernel.Start();

    // due to 1 tick delayed start of the task Kernel enters into a SLEEPING state
    CHECK_EQUAL(platform->m_stack_active, platform->m_stack_info[STACK_SLEEP_TRAP].stack);

    platform->ProcessTick();

    // after a tick task become active and Kernel enters into a AWAKENING state
    CHECK_EQUAL(platform->m_stack_idle, platform->m_stack_info[STACK_SLEEP_TRAP].stack);
    CHECK_EQUAL(platform->m_stack_active->SP, (size_t)task.GetStack());
}

} // namespace stk
} // namespace test
