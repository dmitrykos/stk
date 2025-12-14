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
// ============================ SwitchStrategyRoundRobin ====================== //
// ============================================================================ //

TEST_GROUP(SwitchStrategyRoundRobin)
{
    void setup() {}
    void teardown() {}
};

TEST(SwitchStrategyRoundRobin, GetFirstEmpty)
{
    SwitchStrategyRoundRobin rr;

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

TEST(SwitchStrategyRoundRobin, GetNextEmpty)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1;
    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

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

TEST(SwitchStrategyRoundRobin, EndlessNext)
{
    Kernel<KERNEL_DYNAMIC, 3, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;

    kernel.Initialize();
    kernel.AddTask(&task1);

    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    IKernelTask *next = strategy->GetFirst();
    CHECK_TEXT(strategy->GetNext(next) == next, "Expecting the same next task1 (endless looping)");

    kernel.AddTask(&task2);

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2");

    kernel.AddTask(&task3);

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Expecting the next task3");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Expecting the next task1 (endless looping)");

    IKernelTask *ktask1 = next;
    next = strategy->GetNext(ktask1);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2");

    kernel.RemoveTask(&task1);

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Expecting the next task3");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2 (endless looping)");
}

TEST(SwitchStrategyRoundRobin, Algorithm)
{
    // Create kernel with 3 tasks
    Kernel<KERNEL_DYNAMIC, 3, SwitchStrategyRoundRobin, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;

    kernel.Initialize();

    // Add tasks
    kernel.AddTask(&task1);

    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    IKernelTask *next = strategy->GetFirst();

    // --- Stage 1: 1 task only ---------------------------------------------

    // Always returns the same task
    for (int i = 0; i < 5; i++)
    {
        next = strategy->GetNext(next);
        CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Single task must always be selected");
    }

    // --- Stage 2: add second task -----------------------------------------

    kernel.AddTask(&task2);

    next = strategy->GetNext(next); // should return task2
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Next task should be task2");

    next = strategy->GetNext(next); // should wrap around to task1
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Next task should wrap to task1");

    // --- Stage 3: add third task ------------------------------------------

    kernel.AddTask(&task3);

    // Expected sequence: task1 -> task2 -> task3 -> task1 ...
    next = strategy->GetNext(next); // task2
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Next task should be task2");
    next = strategy->GetNext(next); // task3
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Next task should be task3");
    next = strategy->GetNext(next); // task1
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Next task should wrap to task1");

    // --- Stage 4: remove a task -------------------------------------------

    kernel.RemoveTask(&task2);

    // Expected sequence: task1 -> task3 -> task1 -> task3 ...
    next = strategy->GetNext(next); // task3
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Next task should be task3 after removal");
    next = strategy->GetNext(next); // task1
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Next task should be task1 after removal");
    next = strategy->GetNext(next); // task3
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Next task should wrap to task3");
}

} // namespace stk
} // namespace test
