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
    Kernel<2> kernel;
    PlatformTestMock platform;
    SwitchStrategyRoundRobin switch_strategy;
    TaskMock<ACCESS_USER> task1;
    TaskMock<ACCESS_USER> task2;

    kernel.Initialize(&platform, &switch_strategy);
    kernel.AddTask(&task1);

    IKernelTask *next = switch_strategy.GetFirst();
    CHECK_TRUE_TEXT(switch_strategy.GetNext(next) == next, "Expecting the same next task (endless looping)");

    kernel.AddTask(&task2);

    next = switch_strategy.GetNext(next);
    CHECK_TRUE_TEXT(next->GetUserTask() == &task2, "Expecting the next 2-nd task");

    CHECK_TRUE_TEXT(switch_strategy.GetNext(next)->GetUserTask() == &task1, "Expecting the next 1-st task (endless looping)");
}
