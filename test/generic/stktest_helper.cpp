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
// ============================= UserTask ===================================== //
// ============================================================================ //

TEST_GROUP(UserTask)
{
    void setup() {}
    void teardown() {}
};

TEST(UserTask, GetStackSize)
{
    TaskMock<ACCESS_USER> task;

    CHECK_EQUAL(TaskMock<ACCESS_USER>::STACK_SIZE, task.GetStackSize());
}
