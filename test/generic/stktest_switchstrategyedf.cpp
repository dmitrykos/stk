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
// ============================ SwitchStrategyEDF ====================== //
// ============================================================================ //

TEST_GROUP(SwitchStrategyEDF)
{
    void setup() {}
    void teardown() {}
};

TEST(SwitchStrategyEDF, GetFirstEmpty)
{
    SwitchStrategyEDF rr;

    try
    {
        g_TestContext.ExpectAssert(true);
        rr.GetFirst();
        CHECK_TEXT(false, "expecting assertion when empty");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(SwitchStrategyEDF, GetNextEmpty)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyEDF, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1;
    const ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    kernel.Initialize();

    kernel.AddTask(&task1);
    IKernelTask *ktask = strategy->GetFirst();
    kernel.RemoveTask(&task1);
    CHECK_EQUAL(0, strategy->GetSize());

    try
    {
        g_TestContext.ExpectAssert(true);
        strategy->GetNext(ktask);
        CHECK_TEXT(false, "expecting assertion when empty");
    }
    catch (TestAssertPassed &pass)
    {
        CHECK(true);
        g_TestContext.ExpectAssert(false);
    }
}

TEST(SwitchStrategyEDF, PriorityNext)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 3, SwitchStrategyEDF, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;
    const ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    kernel.Initialize();

    // periodicity, deadline, start_delay
    kernel.AddTask(&task1, 300, 300, 0); // latest deadline
    kernel.AddTask(&task2, 200, 200, 0);
    kernel.AddTask(&task3, 100, 100, 0); // earliest deadline

    // EDF must select the task with the earliest relative deadline
    IKernelTask *current = strategy->GetFirst();
    IKernelTask *next = strategy->GetNext(current);

    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "EDF must select task with earliest relative deadline");

    // Repeated GetNext must still select the same task
    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "EDF must continue selecting earliest-deadline task");

    // Remove earliest-deadline task
    kernel.RemoveTask(&task3);

    next = strategy->GetNext(strategy->GetFirst());
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "after removal, task2 has earliest relative deadline");

    // Remove next earliest
    kernel.RemoveTask(&task2);

    next = strategy->GetNext(strategy->GetFirst());
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "only remaining task must be selected");
}

TEST(SwitchStrategyEDF, Algorithm)
{
    Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 3, SwitchStrategyEDF, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;
    const ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    kernel.Initialize();

    // --- Stage 1: Add first task -----------------------------------------

    kernel.AddTask(&task1, 300, 300, 0);

    IKernelTask *next = strategy->GetFirst();

    // Single task -> always returned
    for (int32_t i = 0; i < 5; i++)
    {
        next = strategy->GetNext(next);
        CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "single task must always be selected");
    }

    // --- Stage 2: Add second task ----------------------------------------

    kernel.AddTask(&task2, 200, 200, 0); // earlier deadline than task1

    // EDF must pick the task with earliest relative deadline
    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "task2 with earlier deadline should preempt task1");

    // Still highest-priority task keeps running until removed or asleep
    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "task2 remains earliest-deadline task");

    // --- Stage 3: Add third task -----------------------------------------

    kernel.AddTask(&task3, 100, 100, 0); // earliest deadline

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "task3 with earliest deadline should run first");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "task3 continues to run as earliest-deadline task");

    // --- Stage 4: Remove tasks -------------------------------------------

    kernel.RemoveTask(&task3);

    next = strategy->GetNext(strategy->GetFirst());
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "task2 becomes earliest-deadline after task3 removal");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "task2 continues as earliest-deadline task");

    kernel.RemoveTask(&task2);

    next = strategy->GetNext(strategy->GetFirst());
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "task1 remains as only task");
}

} // namespace stk
} // namespace test
