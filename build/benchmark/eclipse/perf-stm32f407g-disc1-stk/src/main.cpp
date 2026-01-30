/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <stk_config.h>
#include <stk.h>
#include "perf.h"

using namespace stk;

#define SLEEP_GRANULARITY (_STK_BENCH_WINDOW + 2)

static Kernel<KERNEL_DYNAMIC | KERNEL_SYNC, _STK_BENCH_TASK_MAX + 1, SwitchStrategyRR, PlatformDefault> g_Kernel;
static volatile uint32_t g_Ticks = 0;
static volatile bool g_Enable = false;

extern "C" void SysTick_Handler()
{
    //HAL_IncTick();

    if (g_Enable)
        ++g_Ticks;

    if (g_Kernel.IsStarted())
        g_Kernel.GetPlatform()->ProcessTick();
}

class BenchTask : public Task<_STK_BENCH_STACK_SIZE, ACCESS_PRIVILEGED>
{
public:
    BenchTask() : m_id(~0), m_exited(false) {}
    RunFuncType GetFunc() { return forced_cast<RunFuncType>(&BenchTask::RunInner); }
    void *GetFuncUserData() { return this; }

    void Initialize(uint8_t id) { m_id = id; }
    bool IsExited() const { return m_exited; }

private:
    void RunInner()
    {
        uint32_t index = m_id;

        g_Enable = true;

        while (g_Ticks < _STK_BENCH_WINDOW)
        {
            g_Bench[index].Process();
        }

        m_exited = true;
    }

    uint8_t       m_id;
    volatile bool m_exited;
};

static BenchTask g_Tasks[_STK_BENCH_TASK_MAX];

class ResultTask : public Task<_STK_BENCH_STACK_SIZE, ACCESS_PRIVILEGED>
{
public:
    RunFuncType GetFunc() { return forced_cast<RunFuncType>(&ResultTask::RunInner); }
    void *GetFuncUserData() { return this; }

private:
    void RunInner()
    {
        while (g_Ticks < _STK_BENCH_WINDOW + 2)
        {
            stk::Sleep(SLEEP_GRANULARITY);
        }

    wait:
        for (int32_t i = 0; i < _STK_BENCH_TASK_MAX; ++i)
        {
            if (!g_Tasks[i].IsExited())
                goto wait;
        }

        Crc32Bench::ShowResults();
    }
};

static ResultTask g_TaskResult;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    g_Kernel.Initialize();

    for (int32_t i = 0; i < _STK_BENCH_TASK_MAX; ++i)
    {
        g_Bench[i].Initialize();
        g_Tasks[i].Initialize(i);
        g_Kernel.AddTask(&g_Tasks[i]);
    }

    g_Kernel.AddTask(&g_TaskResult);

    g_Kernel.Start();
    for (;;);
    return 0;
}

