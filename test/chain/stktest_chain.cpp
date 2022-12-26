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

#define _STK_CHAIN_TEST_TASKS_MAX 3

namespace stk {
namespace test {

/*! \namespace stk::test::chain
    \brief     Namespace of Switch test.
 */
namespace chain {

static volatile uint8_t g_TaskSwitch = 0;

/*! \class TestTask
    \brief Switch test task.
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

            printf("id=%d\n", task_id);

            g_KernelService->Delay(100);

            // activate next task and exit
            g_TaskSwitch = (task_id + 1) % 3;
            return;
        }
    }
};

} // namespace chain
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
    using namespace stk::test::chain;

    static Kernel<KERNEL_DYNAMIC, _STK_CHAIN_TEST_TASKS_MAX> kernel;
    static PlatformDefault platform;
    static SwitchStrategyRoundRobin tsstrategy;
    static TestTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start();

    TestContext::ShowTestSuiteEpilogue(TestContext::SUCCESS_EXIT_CODE);
    return TestContext::SUCCESS_EXIT_CODE;
}
