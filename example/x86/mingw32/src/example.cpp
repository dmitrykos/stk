/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#define _STK_ARCH_WIN32_M
#include <stdio.h>
#include <stk.h>

static volatile uint8_t g_TaskSwitch = 0;

template <stk::EAccessMode _AccessMode>
class Task : public stk::UserTask<256, _AccessMode>
{
    uint8_t m_taskId;

public:
    Task(uint8_t taskId) : m_taskId(taskId)
    { }

    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((Task *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t task_id = m_taskId;

        float count = 0;
        uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                ++count_skip;
                continue;
            }

            ++count;

            printf("task: %d\n", task_id);

            stk::g_Kernel->DelaySpin(1000);

            g_TaskSwitch = task_id + 1;
            if (g_TaskSwitch > 2)
                g_TaskSwitch = 0;
        }
    }
};

void RunExample()
{
    using namespace stk;

    static Kernel<10> kernel;
    static PlatformX86Win32 platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static Task<ACCESS_PRIVILEGED> task1(0);
    static Task<ACCESS_USER> task2(1);
    static Task<ACCESS_USER> task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(1000);

    //assert(false);
    while (true);
}

int main()
{
	RunExample();
	return 0;
}
