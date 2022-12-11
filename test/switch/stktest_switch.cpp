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

#define _STK_TEST_TASKS_MAX 3
#define _STK_TEST_CYCLES_MAX 2

static volatile uint8_t g_TaskSwitch = 0;
static volatile uint8_t g_Cycles[_STK_TEST_TASKS_MAX] = {};

template <EAccessMode _AccessMode>
class TestTask : public Task<64, _AccessMode>
{
    uint8_t m_task_id;

public:
    TestTask(uint8_t task_id) : m_task_id(task_id) {}
    RunFuncType GetFunc() { return forced_cast<RunFuncType>(&TestTask::RunInner); }
    void *GetFuncUserData() { return this; }

private:
    void RunInner()
    {
        uint8_t task_id = m_task_id;
        volatile float count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                ++count_skip;
                continue;
            }

            ++g_Cycles[task_id];
            //printf("c=%d id=%d\n", g_Cycles[task_id], task_id);

            // count total workload of all tasks
            uint32_t total = 0;
            for (int32_t i = 0; i < _STK_TEST_TASKS_MAX; ++i)
            {
                total += g_Cycles[i];
            }

            // check if it is a time to evaluate workload counters
            if (total >= (_STK_TEST_CYCLES_MAX * _STK_TEST_TASKS_MAX))
            {
                STK_TEST_CHECK_EQUAL((_STK_TEST_CYCLES_MAX * _STK_TEST_TASKS_MAX), total);

                // check if workload is spread equally between all tasks
                for (int32_t i = 0; i < _STK_TEST_TASKS_MAX; ++i)
                {
                    STK_TEST_CHECK_EQUAL(_STK_TEST_CYCLES_MAX, g_Cycles[i]);
                }

                // success, exit process
                TestContext::ForceExitTestSuie(TestContext::SUCCESS_EXIT_CODE);
                break;
            }

            g_KernelService->DelaySpin(100);

            g_TaskSwitch = task_id + 1;
            if (g_TaskSwitch > 2)
                g_TaskSwitch = 0;
        }
    }
};

/*! \fn    main
    \brief Counts number of workloads processed by each task.
*/
int main()
{
    using namespace stk;

    static Kernel<3> kernel;
    static PlatformDefault platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static TestTask<ACCESS_PRIVILEGED> task1(0);
    static TestTask<ACCESS_PRIVILEGED> task2(1);
    static TestTask<ACCESS_PRIVILEGED> task3(2);

    TestContext::ShowTestSuitePrologue();

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(DEFAULT_RESOLUTION_US_ONE_MSEC);

    return TestContext::DEFAULT_FAILURE_EXIT_CODE;
}
