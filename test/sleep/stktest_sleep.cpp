/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk_config.h>
#include <stk.h>
#include <assert.h>

#include "stktest_context.h"

using namespace stk;
using namespace stk::test;

STK_TEST_DECL_ASSERT;

#define _STK_SLEEP_TEST_TASKS_MAX 3
#define _STK_SLEEP_TEST_SLEEP_TIME 100

namespace stk {
namespace test {

/*! \namespace stk::test::sleep
    \brief     Namespace of Switch test.
 */
namespace sleep {

static int32_t g_Time[_STK_SLEEP_TEST_TASKS_MAX] = {0};

/*! \class TestTask
    \brief Sleep test task.
    \note  Tests sleep capability of the Kernel.
*/
template <stk::EAccessMode _AccessMode>
class TestTask : public stk::Task<256, _AccessMode>
{
    uint8_t m_task_id;

public:
    TestTask(uint8_t task_id) : m_task_id(task_id)
    { }

#if 0
    stk::RunFuncType GetFunc() { return stk::forced_cast<stk::RunFuncType>(&TestTask::RunInner); }
#else
    stk::RunFuncType GetFunc() { return &Run; }
#endif
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((TestTask *)user_data)->RunInner();
    }

    static int64_t GetMilliseconds()
    {
        return (g_KernelService->GetTicks() * g_KernelService->GetTickResolution()) / 1000;
    }

    void RunInner()
    {
        // task 0: sleep 100 ms
        // task 1: sleep 200 ms
        // task 2: sleep 300 ms

        int64_t start = GetMilliseconds();

        g_KernelService->Sleep(_STK_SLEEP_TEST_SLEEP_TIME * (m_task_id + 1));

        int64_t diff = GetMilliseconds() - start;

        printf("id=%d time=%d\n", m_task_id, (int)diff);

        g_Time[m_task_id] = diff;
    }
};

} // namespace sleep
} // namespace test
} // namespace stk

/*! \fn    main
    \brief Counts number of workloads processed by each task.
*/
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    TestContext::ShowTestSuitePrologue();

    using namespace stk;
    using namespace stk::test;
    using namespace stk::test::sleep;

    static Kernel<KERNEL_DYNAMIC, _STK_SLEEP_TEST_TASKS_MAX> kernel;
    static PlatformDefault platform;
    static SwitchStrategyRoundRobin tsstrategy;
    static TestTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start();

    int32_t result = TestContext::SUCCESS_EXIT_CODE;
    for (int32_t i = 0; i < _STK_SLEEP_TEST_TASKS_MAX; ++i)
    {
        int32_t diff = g_Time[i] - ((i + 1) * _STK_SLEEP_TEST_SLEEP_TIME);

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