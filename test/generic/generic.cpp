/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stdio.h>
#include <CppUTest/CommandLineTestRunner.h>

#define _STK_ARCH_X86_WIN32
#include <stk.h>
using namespace stk;

int main()
{
	return RUN_ALL_TESTS(0, (char **)NULL);
}

/*! \class TestContext
    \brief Common context for executed tests.
*/
static class TestContext
{
public:
	TestContext() : m_expect_assert(false)
	{ }

	void ExpectAssert(bool expect) { m_expect_assert = expect; }
	bool IsExpectingAssert() const { return m_expect_assert; }

private:
	bool m_expect_assert;
}
g_TestContext;

/*! \class TestAssertPassed
    \brief Throwable class for catching assertions from _STK_ASSERT_IMPL().
*/
struct TestAssertPassed : public std::exception
{
	const char *what() const noexcept { return "STK test suite exception (TestAssertPassed) thrown!"; }
};

/*! \fn    _STK_ASSERT_IMPL
    \brief Custom assertion handler which intercepts assertions from STK package.
*/
extern void _STK_ASSERT_IMPL(const char *message, const char *file, int32_t line)
{
	if (g_TestContext.IsExpectingAssert())
		throw TestAssertPassed();

	SimpleString what = "Assertion failed!\n";
	what += "\twhat: ";
	what += message;
	what += "\n\tfile: ";
	what += file;
	what += "\n\tline: ";
	what += StringFrom(line);

	CHECK_TEXT(false, what.asCharString());
}

/*! \class PlatformTestMock
    \brief IPlatform mock.
*/
class PlatformTestMock : public IPlatform
{
public:
	PlatformTestMock() : m_resolution(0), m_access_mode(ACCESS_USER)
	{ }
	virtual ~PlatformTestMock()
	{ }
    void Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *firstTask)
    {
    	m_resolution = resolution_us;
    }
    bool InitStack(Stack *stack, ITask *userTask)
    {
    	return true;
    }
    void SwitchContext()
    {

    }
    int32_t GetTickResolution() const
    {
    	return m_resolution;
    }
    void SetAccessMode(EAccessMode mode)
    {
    	m_access_mode = mode;
    }

    int32_t     m_resolution;
    EAccessMode m_access_mode;
};

/*! \class TaskMock
    \brief User task mock.
*/
template <stk::EAccessMode _AccessMode>
class TaskMock : public stk::Task<256, _AccessMode>
{
public:
	TaskMock() : m_started(false) { }
    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

    bool m_started;
private:
    static void Run(void *user_data)
    {
        ((TaskMock *)user_data)->RunInner();
    }

    void RunInner()
    {
    	m_started = 0;
    }
};

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

// ============================================================================ //
// ============================ SwitchStrategyRoundRobin ====================== //
// ============================================================================ //

TEST_GROUP(TestSwitchStrategyRoundRobin)
{
	void setup() {}
	void teardown() {}
};

TEST(TestSwitchStrategyRoundRobin, EndlessNext)
{
	Kernel<2> kernel;
	PlatformTestMock platform;
	SwitchStrategyRoundRobin switch_strategy;
	TaskMock<stk::ACCESS_USER> task1;
	TaskMock<stk::ACCESS_USER> task2;

	kernel.Initialize(&platform, &switch_strategy);
	kernel.AddTask(&task1);

	IKernelTask *next = switch_strategy.GetFirst();
	CHECK_TRUE_TEXT(switch_strategy.GetNext(next) == next, "Expecting the same next task (endless looping)");

	kernel.AddTask(&task2);

	next = switch_strategy.GetNext(next);
	CHECK_TRUE_TEXT(next->GetUserTask() == &task2, "Expecting the next 2-nd task");

	CHECK_TRUE_TEXT(switch_strategy.GetNext(next)->GetUserTask() == &task1, "Expecting the next 1-st task (endless looping)");
}
