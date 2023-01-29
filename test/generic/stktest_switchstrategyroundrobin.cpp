/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
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
    ITaskSwitchStrategy *strategy = ((IKernel &)kernel).GetSwitchStrategy();

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

    ITaskSwitchStrategy *strategy = ((IKernel &)kernel).GetSwitchStrategy();

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

} // namespace stk
} // namespace test
