/*
 * SuperTinyKernel(TM) RTOS: Lightweight High-Performance Deterministic C++ RTOS for Embedded Systems.
 *
 * Source: https://github.com/SuperTinyKernel-RTOS
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>. All Rights Reserved.
 * License: MIT License, see LICENSE for a full text.
 */

/*! \file  stktest_hrt.cpp
    \brief Test suite for the Hard Real-Time (HRT) mode of the kernel (stk::KERNEL_HRT).

    Every test registers 1..4 HRT tasks (periodicity / deadline / start delay) whose Run() performs
    a configurable amount of work with Delay() and then calls Yield() to signal job completion.
    Start times and work durations of every job are recorded and verified after Kernel::Start()
    returns (KERNEL_DYNAMIC: Start() returns when all tasks have exited).

    Scenarios covered by the suite:
     - Basic:             3 tasks, staggered start delays, varying workload (the original test);
     - SingleTask:        one task alone in the system (sleep trap is entered every period);
     - ZeroWork:          a job that completes immediately (Yield() without any work);
     - Simultaneous:      several tasks activated at the same moment (deadline covers waiting time);
     - TightDeadline:     deadline only a few ticks above the workload;
     - LongStartDelay:    start delay longer than the task's own period;
     - DifferentPeriods:  tasks with different periodicities and workloads running side by side;
     - NoDrift:           many periods of a single task: no accumulated drift (period is exact);
     - StaggeredExit:     tasks exit after different number of jobs, the rest keep their schedule;
     - ExitWithoutYield:  the last job ends by returning from Run() instead of Yield();
     - ImmediateExit:     a task returns from Run() without running a single job;
     - FullPool:          all kernel task slots are used;
     - DeadlineMiss:      an overrun is detected (strict "duration > deadline" rule) and reported
                          through ITask::OnDeadlineMissed() + IEventOverrider::OnHardFault();
     - MissIsolated:      a missing task does not disturb the schedule of a healthy one.

    Timing notes (tick = 1 ms, the default): Delay() is wall-clock based, therefore the work of tasks
    which are active at the same time overlaps instead of being serialized. A job lasts at least
    its workload (minus one tick of measurement jitter) and typically a few ticks more.

    Deadline-miss scenarios need the hard fault to be survivable: the test installs an
    IPlatform::IEventOverrider whose OnHardFault() counts the fault and reports it as handled,
    so the platform driver does not halt the system. In all other scenarios the fault counter
    must stay zero.
*/

#include <stk_config.h>
#include <stk.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "stktest_context.h"

using namespace stk;
using namespace stk::test;

STK_TEST_DECL_ASSERT;

#define _STK_HRT_TEST_TASKS_MAX      4
#define _STK_HRT_TEST_ITRS_MAX       20   // max jobs recorded per task
#define _STK_HRT_TEST_START_TOL_MS   10   // allowed deviation of a job start from its expected time (ms)
#define _STK_HRT_TEST_WORK_LO_MS     1    // allowed "early" slack of a measured workload (ms)
#define _STK_HRT_TEST_WORK_HI_MS     10   // allowed "late" slack of a measured workload (ms)

#if defined(_STK_ARCH_RISC_V)
#define _STK_HRT_STACK_SIZE          384
#else
#define _STK_HRT_STACK_SIZE          256
#endif

namespace stk {
namespace test {

/*! \namespace stk::test::hrt
    \brief     Namespace of HRT test.
 */
namespace hrt {

/*! \struct TaskCfg
    \brief  Per-task configuration of a scenario (all times in milliseconds).
*/
struct TaskCfg
{
    int32_t period;       //!< HRT periodicity.
    int32_t deadline;     //!< HRT deadline.
    int32_t delay;        //!< Start delay.
    int32_t work;         //!< Workload of one job (busy delay).
    int32_t iterations;   //!< Number of jobs before the task exits.
    bool    yield_last;   //!< Yield() after the last job (false: return from Run() right away).
    bool    expect_miss;  //!< The workload exceeds the deadline: a deadline miss is expected.
};

// Test state (every array element has a single writer)
static TaskCfg          g_Cfg[_STK_HRT_TEST_TASKS_MAX] = {};
static volatile int32_t g_Start[_STK_HRT_TEST_TASKS_MAX][_STK_HRT_TEST_ITRS_MAX] = {};
static volatile int32_t g_Diff[_STK_HRT_TEST_TASKS_MAX][_STK_HRT_TEST_ITRS_MAX] = {};
static volatile int32_t g_Count[_STK_HRT_TEST_TASKS_MAX] = {};      // completed jobs
static volatile int32_t g_MissCount[_STK_HRT_TEST_TASKS_MAX] = {};  // OnDeadlineMissed() calls
static volatile int32_t g_MissFirst[_STK_HRT_TEST_TASKS_MAX] = {};  // duration reported by the first call

// Kernel
static Kernel<KERNEL_DYNAMIC | KERNEL_HRT, _STK_HRT_TEST_TASKS_MAX, SwitchStrategyRoundRobin, PlatformDefault> g_Kernel;

/*! \class HrtOverrider
    \brief Makes a hard fault (HRT deadline miss) survivable and countable.
*/
class HrtOverrider : public IPlatform::IEventOverrider
{
public:
    HrtOverrider() : m_faults(0)
    {}

    bool OnHardFault() override
    {
        ++m_faults;
        return true; // handled: do not halt the system
    }

    volatile int32_t m_faults;
};

static HrtOverrider g_Overrider;

static Timeout MsToTicks(int32_t ms)
{
    return static_cast<Timeout>(GetTicksFromMs(ms, static_cast<uint32_t>(PERIODICITY_DEFAULT)));
}

static int32_t Abs(int32_t v)
{
    return ((v < 0) ? -v : v);
}

static bool InRange(int32_t v, int32_t lo, int32_t hi)
{
    return ((v >= lo) && (v <= hi));
}

/*! \class HrtTask
    \brief HRT test task: runs cfg.iterations jobs, each is a busy delay of cfg.work ms followed by Yield().
    \note  Sleep() is not supported in HRT mode, the workload is simulated with Delay() (busy-wait),
           and the task is put to sleep until its next period by the kernel on Yield().
*/
template <EAccessMode _AccessMode>
class HrtTask : public Task<_STK_HRT_STACK_SIZE, _AccessMode>
{
    uint8_t m_task_id;

public:
    HrtTask(uint8_t task_id) : m_task_id(task_id)
    {}

    // called from the tick handler: record only
    void OnDeadlineMissed(uint32_t duration) override
    {
        if (g_MissCount[m_task_id] == 0)
            g_MissFirst[m_task_id] = static_cast<int32_t>(duration);

        g_MissCount[m_task_id] = g_MissCount[m_task_id] + 1;
    }

private:
    void Run()
    {
        const TaskCfg &cfg = g_Cfg[m_task_id];

        for (int32_t i = 0; i < cfg.iterations; ++i)
        {
            const int64_t start = GetTimeNowMs();

            if (cfg.work > 0)
                stk::Delay(MsToTicks(cfg.work));

            const int64_t diff = GetTimeNowMs() - start;

            g_Start[m_task_id][i] = static_cast<int32_t>(start);
            g_Diff[m_task_id][i]  = static_cast<int32_t>(diff);
            g_Count[m_task_id]    = g_Count[m_task_id] + 1;

            // signal job completion, the kernel sleeps the task until its next period
            if (((i + 1) < cfg.iterations) || cfg.yield_last)
                stk::Yield();
        }
    }
};

/*! \enum  EScenario
    \brief Test scenarios, see the file note.
*/
enum EScenario : int32_t
{
    SC_BASIC = 0,
    SC_SINGLE_TASK,
    SC_ZERO_WORK,
    SC_SIMULTANEOUS,
    SC_TIGHT_DEADLINE,
    SC_LONG_START_DELAY,
    SC_DIFFERENT_PERIODS,
    SC_NO_DRIFT,
    SC_STAGGERED_EXIT,
    SC_EXIT_WITHOUT_YIELD,
    SC_IMMEDIATE_EXIT,
    SC_FULL_POOL,
    SC_DEADLINE_MISS,
    SC_MISS_ISOLATED
};

static void Cfg(int32_t id, int32_t period, int32_t deadline, int32_t delay, int32_t work, int32_t iterations,
    bool yield_last = true, bool expect_miss = false)
{
    STK_ASSERT(iterations <= _STK_HRT_TEST_ITRS_MAX);

    TaskCfg &c = g_Cfg[id];

    c.period      = period;
    c.deadline    = deadline;
    c.delay       = delay;
    c.work        = work;
    c.iterations  = iterations;
    c.yield_last  = yield_last;
    c.expect_miss = expect_miss;
}

//! Fill g_Cfg for a scenario, returns the number of tasks.
static int32_t SetupScenario(int32_t scenario)
{
    int32_t tasks = 0;

    switch (scenario)
    {
    case SC_BASIC:
        // tasks activated one after another every 100 ms, varying workload 10/20/30 ms, deadline 50 ms
        Cfg(0, 300, 50, 0,   10, 3);
        Cfg(1, 300, 50, 100, 20, 3);
        Cfg(2, 300, 50, 200, 30, 3);
        tasks = 3;
        break;
    case SC_SINGLE_TASK:
        Cfg(0, 50, 25, 0, 5, 5);
        tasks = 1;
        break;
    case SC_ZERO_WORK:
        // job completes at once: the whole period is spent sleeping
        Cfg(0, 20, 10, 0, 0, 6);
        tasks = 1;
        break;
    case SC_SIMULTANEOUS:
        // all tasks become active at the same time (the deadline also covers the time spent waiting for the CPU)
        Cfg(0, 60, 40, 0, 10, 3);
        Cfg(1, 60, 40, 0, 15, 3);
        Cfg(2, 60, 40, 0, 20, 3);
        tasks = 3;
        break;
    case SC_TIGHT_DEADLINE:
        // deadline is only 8 ms above the workload
        Cfg(0, 100, 28, 0,  20, 4);
        Cfg(1, 100, 28, 50, 20, 4);
        tasks = 2;
        break;
    case SC_LONG_START_DELAY:
        // task 1 first starts after 200 ms: longer than its own period (80 ms)
        Cfg(0, 50, 20, 0,   5,  8);
        Cfg(1, 80, 30, 200, 10, 2);
        tasks = 2;
        break;
    case SC_DIFFERENT_PERIODS:
        Cfg(0, 40,  30, 0,  5,  6);
        Cfg(1, 60,  30, 10, 8,  4);
        Cfg(2, 100, 30, 20, 12, 3);
        tasks = 3;
        break;
    case SC_NO_DRIFT:
        // 20 periods: every start must stay on the (start + i * period) grid
        Cfg(0, 20, 10, 0, 3, 20);
        tasks = 1;
        break;
    case SC_STAGGERED_EXIT:
        // tasks exit after 1, 3 and 6 jobs, the remaining ones must keep their schedule
        Cfg(0, 30, 20, 0,  5, 1);
        Cfg(1, 30, 20, 10, 5, 3);
        Cfg(2, 30, 20, 20, 5, 6);
        tasks = 3;
        break;
    case SC_EXIT_WITHOUT_YIELD:
        // the last job ends by returning from Run() (no final Yield())
        Cfg(0, 40, 20, 0,  5, 3, false);
        Cfg(1, 40, 20, 20, 5, 3, false);
        tasks = 2;
        break;
    case SC_IMMEDIATE_EXIT:
        // task 0 returns from Run() without a single job
        Cfg(0, 40, 20, 0, 5, 0);
        Cfg(1, 40, 20, 0, 5, 3);
        tasks = 2;
        break;
    case SC_FULL_POOL:
        // all kernel slots are used, activations are spread across the common period
        Cfg(0, 200, 40, 0,   10, 3);
        Cfg(1, 200, 40, 50,  20, 3);
        Cfg(2, 200, 40, 100, 30, 3);
        Cfg(3, 200, 40, 150, 10, 3);
        tasks = 4;
        break;
    case SC_DEADLINE_MISS:
        // workload 40 ms vs deadline 20 ms: the first overrun is detected at duration == deadline + 1
        Cfg(0, 100, 20, 0, 40, 2, true, true);
        tasks = 1;
        break;
    case SC_MISS_ISOLATED:
        // task 0 overruns, task 1 must not be affected
        Cfg(0, 100, 20, 0,  40, 3, true, true);
        Cfg(1, 100, 30, 50, 10, 3);
        tasks = 2;
        break;
    default:
        break;
    }

    return tasks;
}

// Helper function to reset test state
static void ResetTestState()
{
    memset(g_Cfg, 0, sizeof(g_Cfg));

    for (int32_t t = 0; t < _STK_HRT_TEST_TASKS_MAX; ++t)
    {
        for (int32_t i = 0; i < _STK_HRT_TEST_ITRS_MAX; ++i)
        {
            g_Start[t][i] = 0;
            g_Diff[t][i]  = 0;
        }

        g_Count[t]     = 0;
        g_MissCount[t] = 0;
        g_MissFirst[t] = 0;
    }

    g_Overrider.m_faults = 0;
}

/*! \brief     Verify the recorded schedule of a scenario.
    \note      The kernel tick counter keeps running between tests, so expected times are relative to
               a base derived from the first job of the first task which ran at least one job:
               base = start[0] - delay. Expected start of job i: base + delay + i * period.
*/
static bool VerifyScenario(int32_t tasks)
{
    bool ok = true;
    bool any_miss = false;

    int32_t ref = -1;
    for (int32_t t = 0; t < tasks; ++t)
    {
        if ((g_Cfg[t].iterations > 0) && (g_Count[t] > 0))
        {
            ref = t;
            break;
        }
    }

    if (ref < 0)
    {
        printf("hrt: no job was executed\n");
        return false;
    }

    const int32_t base = g_Start[ref][0] - g_Cfg[ref].delay;

    for (int32_t t = 0; t < tasks; ++t)
    {
        const TaskCfg &c = g_Cfg[t];

        if (g_Count[t] != c.iterations)
        {
            printf("hrt: id=%d jobs done=%d (expected %d)\n", (int)t, (int)g_Count[t], (int)c.iterations);
            ok = false;
        }

        for (int32_t i = 0; i < g_Count[t]; ++i)
        {
            const int32_t expect     = base + c.delay + (i * c.period);
            const int32_t start      = g_Start[t][i];
            const int32_t diff       = g_Diff[t][i];
            const int32_t diff_start = Abs(start - expect);

            printf("id=%d itr=%d start=%d (expect %d) diff=%d (work %d)\n",
                (int)t, (int)i, (int)(start - base), (int)(expect - base), (int)diff, (int)c.work);

            if (diff_start > _STK_HRT_TEST_START_TOL_MS)
            {
                printf("failed start (%d): id=%d diff_start=%d (>%d)\n", (int)i, (int)t,
                    (int)diff_start, (int)_STK_HRT_TEST_START_TOL_MS);
                ok = false;
            }

            if (!InRange(diff, c.work - _STK_HRT_TEST_WORK_LO_MS, c.work + _STK_HRT_TEST_WORK_HI_MS))
            {
                printf("failed work (%d): id=%d diff=%d work=%d\n", (int)i, (int)t, (int)diff, (int)c.work);
                ok = false;
            }

            // a healthy job completes within its deadline
            if (!c.expect_miss && (diff > c.deadline))
            {
                printf("failed deadline (%d): id=%d diff=%d deadline=%d\n", (int)i, (int)t, (int)diff,
                    (int)c.deadline);
                ok = false;
            }
        }

        if (c.expect_miss)
        {
            any_miss = true;

            // detection rule is strict (duration > deadline): the first report is deadline + 1
            printf("hrt: id=%d misses=%d first_duration=%d (expected %d)\n", (int)t, (int)g_MissCount[t],
                (int)g_MissFirst[t], (int)(c.deadline + 1));

            if ((g_MissCount[t] == 0) || (g_MissFirst[t] != (c.deadline + 1)))
                ok = false;
        }
        else if (g_MissCount[t] != 0)
        {
            printf("hrt: id=%d unexpected deadline miss (%d), first duration=%d\n", (int)t,
                (int)g_MissCount[t], (int)g_MissFirst[t]);
            ok = false;
        }
        else
        {
            // no miss expected and none reported
        }
    }

    printf("hrt: hard faults=%d (%s)\n", (int)g_Overrider.m_faults, any_miss ? "expected" : "none expected");

    if (any_miss ? (g_Overrider.m_faults == 0) : (g_Overrider.m_faults != 0))
        ok = false;

    return ok;
}

} // namespace hrt
} // namespace test
} // namespace stk

/*! \fn    RunTest
    \brief Helper function to run a single test case.
    \param[in] test_name: Name of the test.
    \param[in] scenario:  Scenario id (see EScenario), selects the task configuration.
*/
template <class TaskType>
static int32_t RunTest(const char *test_name, int32_t scenario)
{
    using namespace stk;
    using namespace stk::test;
    using namespace stk::test::hrt;

    printf("Test: %s\n", test_name);

    ResetTestState();

    const int32_t tasks = SetupScenario(scenario);
    STK_ASSERT((tasks > 0) && (tasks <= _STK_HRT_TEST_TASKS_MAX));

    // static: a removed task (and its stack) is reused by the next test
    static TaskType task0(0);
    static TaskType task1(1);
    static TaskType task2(2);
    static TaskType task3(3);

    ITask *const list[_STK_HRT_TEST_TASKS_MAX] = { &task0, &task1, &task2, &task3 };

    for (int32_t t = 0; t < tasks; ++t)
    {
        g_Kernel.AddTask(list[t], MsToTicks(g_Cfg[t].period), MsToTicks(g_Cfg[t].deadline),
            MsToTicks(g_Cfg[t].delay));
    }

    g_Kernel.Start();

    int32_t result = (VerifyScenario(tasks) ? TestContext::SUCCESS_EXIT_CODE : TestContext::DEFAULT_FAILURE_EXIT_CODE);

    printf("Result: %s\n", result == TestContext::SUCCESS_EXIT_CODE ? "PASS" : "FAIL");
    printf("--------------\n");

    return result;
}

/*! \fn    main
    \brief Entry to the test suite.
*/
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    using namespace stk::test::hrt;

    TestContext::ShowTestSuitePrologue();

    int total_failures = 0, total_success = 0;

    printf("--------------\n");

    g_Kernel.Initialize();

    // hard faults of the HRT mode are counted and survived (must be set before Start())
    g_Kernel.GetPlatform()->SetEventOverrider(&g_Overrider);

#define RUN(TestClass, name, scenario) \
    do { \
        if (RunTest<TestClass<ACCESS_PRIVILEGED>>(name, scenario) \
                != TestContext::SUCCESS_EXIT_CODE) \
            total_failures++; \
        else \
            total_success++; \
    } while (0)

    // Test 1: Basic, 3 tasks with staggered start delays and workloads of 10/20/30 ms
    RUN(HrtTask, "Basic",             SC_BASIC);

    // Test 2: A single task alone in the system (CPU enters the sleep trap every period)
    RUN(HrtTask, "SingleTask",        SC_SINGLE_TASK);

    // Test 3: Job completes immediately, Yield() without any work
    RUN(HrtTask, "ZeroWork",          SC_ZERO_WORK);

    // Test 4: Tasks activated at the same moment, deadline covers the time waiting for the CPU
    RUN(HrtTask, "Simultaneous",      SC_SIMULTANEOUS);

    // Test 5: Deadline only a few ticks above the workload, no miss allowed
    RUN(HrtTask, "TightDeadline",     SC_TIGHT_DEADLINE);

    // Test 6: Start delay longer than the period of the task
    RUN(HrtTask, "LongStartDelay",    SC_LONG_START_DELAY);

    // Test 7: Tasks with different periodicities and workloads
    RUN(HrtTask, "DifferentPeriods",  SC_DIFFERENT_PERIODS);

    // Test 8: 20 consecutive periods, no accumulated drift
    RUN(HrtTask, "NoDrift",           SC_NO_DRIFT);

    // Test 9: Tasks exit after a different number of jobs, the others keep their schedule
    RUN(HrtTask, "StaggeredExit",     SC_STAGGERED_EXIT);

    // Test 10: Last job ends by returning from Run() instead of Yield()
    RUN(HrtTask, "ExitWithoutYield",  SC_EXIT_WITHOUT_YIELD);

    // Test 11: A task returns from Run() without running a single job
    RUN(HrtTask, "ImmediateExit",     SC_IMMEDIATE_EXIT);

    // Test 12: All kernel slots are used
    RUN(HrtTask, "FullPool",          SC_FULL_POOL);

    // Test 13: Deadline overrun is detected (duration == deadline + 1) and reported as a hard fault
    RUN(HrtTask, "DeadlineMiss",      SC_DEADLINE_MISS);

    // Test 14: Overrunning task does not disturb the schedule of a healthy one
    RUN(HrtTask, "MissIsolated",      SC_MISS_ISOLATED);

#undef RUN

    int32_t final_result = (total_failures == 0
        ? TestContext::SUCCESS_EXIT_CODE
        : TestContext::DEFAULT_FAILURE_EXIT_CODE);

    printf("##############\n");
    printf("Total tests: %d\n", total_failures + total_success);
    printf("Failures: %d\n", (int)total_failures);

    TestContext::ShowTestSuiteEpilogue(final_result);
    return final_result;
}
