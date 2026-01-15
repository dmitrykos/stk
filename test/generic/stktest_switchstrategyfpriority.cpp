/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"

namespace stk {
namespace test {

// ============================================================================ //
// ========================== SwitchStrategyFixedPriority ===================== //
// ============================================================================ //

TEST_GROUP(SwitchStrategyFixedPriority)
{
    void setup() {}
    void teardown() {}
};

TEST(SwitchStrategyFixedPriority, GetFirstEmpty)
{
    SwitchStrategyFP31 rr;

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

TEST(SwitchStrategyFixedPriority, GetNextEmpty)
{
    Kernel<KERNEL_DYNAMIC, 1, SwitchStrategyFP31, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1;
    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    kernel.Initialize();

    kernel.AddTask(&task1);
    kernel.RemoveTask(&task1);
    CHECK_EQUAL(0, strategy->GetSize());

    // expect to return NULL which puts core into a sleep mode, current is ignored by this strategy
    CHECK_EQUAL(0, strategy->GetNext(NULL));
}

TEST(SwitchStrategyFixedPriority, EndlessNext)
{
    Kernel<KERNEL_DYNAMIC, 3, SwitchStrategyFP31, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;
    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    IKernelTask *first = strategy->GetFirst();
    CHECK_EQUAL_TEXT(&task1, first->GetUserTask(), "expecting first task1");

    IKernelTask *next = strategy->GetNext(NULL);
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "expecting next task1");

    next = strategy->GetNext(first);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "expecting next task2");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "expecting next task3");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "expecting next task1 again (endless looping)");

    next = strategy->GetNext(first);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "expecting next task2 again (endless looping)");

    kernel.RemoveTask(&task2);

    next = strategy->GetNext(NULL);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "expecting next task3 again (endless looping)");

    next = strategy->GetNext(next);
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "expecting next task1 again (endless looping)");
}

TEST(SwitchStrategyFixedPriority, Algorithm)
{
    // Create kernel with 3 tasks
    Kernel<KERNEL_DYNAMIC, 3, SwitchStrategyFP31, PlatformTestMock> kernel;
    TaskMock<ACCESS_USER> task1, task2, task3;

    kernel.Initialize();

    // Add tasks
    kernel.AddTask(&task1);

    ITaskSwitchStrategy *strategy = kernel.GetSwitchStrategy();

    IKernelTask *next = strategy->GetFirst();

    // --- Stage 1: 1 task only ---------------------------------------------

    // Always returns the same task
    for (int32_t i = 0; i < 5; i++)
    {
        next = strategy->GetNext(next);
        CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Single task must always be selected");
    }

    // --- Stage 2: add second task -----------------------------------------

    kernel.AddTask(&task2);

    next = strategy->GetNext(next); // should still return task1 as task2 will be scheduled after this call
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Next task should be task1");
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
