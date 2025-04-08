/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk_config.h>
#include <stk.h>
#include <assert.h>

#include "stktest_context.h"

using namespace stk;
using namespace stk::test;

STK_TEST_DECL_ASSERT;

#define _STK_CHAIN_TEST_TASKS_MAX  3
#define _STK_CHAIN_TEST_DELAY_TIME 100

namespace stk {
namespace test {

/*! \namespace stk::test::chain
    \brief     Namespace of Chain test.
 */
namespace chain {

static volatile uint8_t g_TaskSwitch = 0;

/*! \class TestTask
    \brief Chain test task.
    \note  Counts __STK_CHAIN_TEST_CYCLES_MAX cycles with _STK_CHAIN_TEST_TASKS_MAX. Succeeds if counter incremented correctly.
*/
template <EAccessMode _AccessMode>
class TestTask : public Task<256, _AccessMode>
{
    uint8_t m_task_id;

public:
    TestTask(uint8_t task_id) : m_task_id(task_id) {}
    RunFuncType GetFunc() { return forced_cast<RunFuncType>(&TestTask::RunInner); }
    void *GetFuncUserData() { return this; }

private:
    void RunInner();
};

static Kernel<KERNEL_DYNAMIC, _STK_CHAIN_TEST_TASKS_MAX, SwitchStrategyRoundRobin, PlatformDefault> kernel;
static TestTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);
static int64_t g_Time[_STK_CHAIN_TEST_TASKS_MAX] = {};

template<> void TestTask<ACCESS_PRIVILEGED>::RunInner()
{
    uint8_t task_id = m_task_id;

    g_Time[task_id] = stk::GetTimeNowMilliseconds();

    printf("id=%d time=%d\n", task_id, (int)g_Time[task_id]);

    g_KernelService->Delay(_STK_CHAIN_TEST_DELAY_TIME);

    // activate next task and exit
    g_TaskSwitch = (task_id + 1) % 3;
    if (g_TaskSwitch == 1)
        kernel.AddTask(&task2);
    else
    if (g_TaskSwitch == 2)
        kernel.AddTask(&task3);
}

} // namespace chain
} // namespace test
} // namespace stk

/*! \fn    main
    \brief Entry to the test case.
*/
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    TestContext::ShowTestSuitePrologue();

    using namespace stk;
    using namespace stk::test;
    using namespace stk::test::chain;

    kernel.Initialize();
    kernel.AddTask(&task1);
    kernel.Start();

    int32_t result = TestContext::SUCCESS_EXIT_CODE;
    for (int32_t i = 0; i < _STK_CHAIN_TEST_TASKS_MAX; ++i)
    {
        int32_t diff = g_Time[i] - (i * _STK_CHAIN_TEST_DELAY_TIME);

        if (diff < 0)
            diff = -diff;

        // check if time difference for every task is not more than 10 ms
        if (diff > 10)
        {
            printf("failed time: id=%d diff=%d (>10)\n", (int)i, (int)diff);
            result = TestContext::DEFAULT_FAILURE_EXIT_CODE;
        }
    }

    TestContext::ShowTestSuiteEpilogue(result);
    return result;
}
