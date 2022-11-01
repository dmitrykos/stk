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
	TaskMock<stk::ACCESS_USER> task;

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
	TaskMock<stk::ACCESS_USER> task;

	kernel.Initialize(&platform, &switch_strategy);

	IKernelTask *ktask = switch_strategy.GetFirst();
	CHECK_TRUE_TEXT(ktask == NULL, "Expecting none kernel tasks");

	kernel.AddTask(&task);

	ktask = switch_strategy.GetFirst();
	CHECK_TRUE_TEXT(ktask != NULL, "Expecting one kernel task");

	CHECK_TRUE_TEXT(ktask->GetUserTask() == &task, "Expecting just added user task");
}

TEST(TestKernel, AddTaskMaxOut)
{
	Kernel<2> kernel;
	PlatformTestMock platform;
	SwitchStrategyRoundRobin switch_strategy;
	TaskMock<stk::ACCESS_USER> task1;
	TaskMock<stk::ACCESS_USER> task2;
	TaskMock<stk::ACCESS_USER> task3;

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
	TaskMock<stk::ACCESS_USER> task;

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
