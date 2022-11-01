
#include <stdio.h>
#include <CppUTest/CommandLineTestRunner.h>

#define _STK_ARCH_X86_WIN32
#include <stk.h>
using namespace stk;

int main()
{
	return RUN_ALL_TESTS(0, (char **)NULL);
}

static bool g_ExpectAssert = false;

struct TestAssertPassed
{
	TestAssertPassed()
	{ }
};

extern void _STK_ASSERT_IMPL(const char *message, const char *file, int32_t line)
{
	if (g_ExpectAssert)
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

template <stk::EAccessMode _AccessMode>
class TestTask : public stk::Task<256, _AccessMode>
{
public:
	TestTask() : m_started(false) { }
    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

    bool m_started;
private:
    static void Run(void *user_data)
    {
        ((TestTask *)user_data)->RunInner();
    }

    void RunInner()
    {
    	m_started = 0;
    }
};

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
		g_ExpectAssert = true;
		kernel.Initialize(NULL, &switch_strategy);
		CHECK_TEXT(false, "Kernel::Initialize() did not fail with platform = NULL");
	}
	catch (TestAssertPassed &pass)
	{
		CHECK(true);
		g_ExpectAssert = false;
	}
}

TEST(TestKernel, InitFailSwitchStrategyNull)
{
	Kernel<10> kernel;
	PlatformTestMock platform;

	try
	{
		g_ExpectAssert = true;
		kernel.Initialize(&platform, NULL);
		CHECK_TEXT(false, "Kernel::Initialize() did not fail with switch_strategy = NULL");
	}
	catch (TestAssertPassed &pass)
	{
		CHECK(true);
		g_ExpectAssert = false;
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
		g_ExpectAssert = true;
		kernel.Initialize(&platform, &switch_strategy);
		kernel.Initialize(&platform, &switch_strategy);
		CHECK_TEXT(false, "duplicate Kernel::Initialize() did not fail");
	}
	catch (TestAssertPassed &pass)
	{
		CHECK(true);
		g_ExpectAssert = false;
	}
}

TEST(TestKernel, AddTaskNoInit)
{
	Kernel<10> kernel;
	TestTask<stk::ACCESS_USER> task;

	try
	{
		g_ExpectAssert = true;
		kernel.AddTask(&task);
	}
	catch (TestAssertPassed &pass)
	{
		CHECK(true);
		g_ExpectAssert = false;
	}
}

TEST(TestKernel, AddTask)
{
	Kernel<10> kernel;
	PlatformTestMock platform;
	SwitchStrategyRoundRobin switch_strategy;
	TestTask<stk::ACCESS_USER> task;

	kernel.Initialize(&platform, &switch_strategy);

	IKernelTask *ktask = switch_strategy.GetFirst();
	CHECK_TRUE_TEXT(ktask == NULL, "Expecting none kernel tasks");

	kernel.AddTask(&task);

	ktask = switch_strategy.GetFirst();
	CHECK_TRUE_TEXT(ktask != NULL, "Expecting one kernel task");

	CHECK_TRUE_TEXT(ktask->GetUserTask() == &task, "Expecting just added user task");
}

TEST_GROUP(TestSwitchStrategyRoundRobin)
{
	void setup() {}
	void teardown() {}
};

TEST(TestSwitchStrategyRoundRobin, EndlessNext)
{
	Kernel<10> kernel;
	PlatformTestMock platform;
	SwitchStrategyRoundRobin switch_strategy;
	TestTask<stk::ACCESS_USER> task;

	kernel.Initialize(&platform, &switch_strategy);
	kernel.AddTask(&task);

	IKernelTask *first = switch_strategy.GetFirst();

	CHECK_TRUE_TEXT(switch_strategy.GetNext(first) == first, "Expecting the same next task (endless looping)");
}
