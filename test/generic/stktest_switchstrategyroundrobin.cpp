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
// ============================ SwitchStrategyRoundRobin ====================== //
// ============================================================================ //

TEST_GROUP(SwitchStrategyRoundRobin)
{
    void setup() {}
    void teardown() {}
};

TEST(SwitchStrategyRoundRobin, EndlessNext)
{
    Kernel<KERNEL_DYNAMIC, 3> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;
    TaskMock<ACCESS_USER> task3;

    kernel.Initialize(&platform, &strategy);
    kernel.AddTask(&task1);

    IKernelTask *next = strategy.GetFirst();
    CHECK_TEXT(strategy.GetNext(next) == next, "Expecting the same next task1 (endless looping)");

    kernel.AddTask(&task2);

    next = strategy.GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2");

    kernel.AddTask(&task3);

    next = strategy.GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Expecting the next task3");

    next = strategy.GetNext(next);
    CHECK_EQUAL_TEXT(&task1, next->GetUserTask(), "Expecting the next task1 (endless looping)");

    IKernelTask *ktask1 = next;
    next = strategy.GetNext(ktask1);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2");

    kernel.RemoveTask(&task1);

    next = strategy.GetNext(next);
    CHECK_EQUAL_TEXT(&task3, next->GetUserTask(), "Expecting the next task3");

    next = strategy.GetNext(next);
    CHECK_EQUAL_TEXT(&task2, next->GetUserTask(), "Expecting the next task2 (endless looping)");
}

