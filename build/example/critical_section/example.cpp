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
#include "example.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // for system Sleep()
#endif

enum 
{
    ITERATIONS_MAX = 1000,
    THREADS_MAX    = 4,
    COUNTER_EXPECT = ITERATIONS_MAX * THREADS_MAX
};

// Our global counter variable shared by all threads
int32_t g_Counter = 0;

static void UnsafeIncrement()
{
    // read current value
    int32_t temp = g_Counter;

    // we are making thread-race with this delay inevitable, we can't invoke stk::Sleep() or stk::Delay()
    // here as it will deadlock program execution because critical section temporarily frozen
    // our scheduler, thus use critical section with care and ONLY if REALLY necessary
    volatile int32_t delay = 1000000;
    while (--delay);

    // it will overwrite the result which was most likely set by another thread
    // therefore counter will not be incremented ITERATIONS_MAX * THREADS_MAX times
    g_Counter = temp + 1;
}

// Function to increment the counter without a critical section
static void IncrementWithoutCS(void *)
{
    for (int32_t i = 0; i < ITERATIONS_MAX; ++i)
    {
        stk::ScopedCriticalSection __cs1;
        stk::ScopedCriticalSection __cs2; // STK supports nesting of critical sections

        UnsafeIncrement();
    }
}

// Function to increment the counter with a critical section
static void IncrementWithCS(void *)
{
    for (int32_t i = 0; i < ITERATIONS_MAX; ++i)
    {
        UnsafeIncrement();
    }
}

static void InitLEDs()
{
    Led::Init(Led::RED, false);
    Led::Init(Led::GREEN, false);
    Led::Init(Led::BLUE, false);
}

// Task's thread object
template <stk::EAccessMode _AccessMode>
class Thread : public stk::Task<256, _AccessMode>
{
public:
    stk::RunFuncType GetFunc() { return m_func; }
    void *GetFuncUserData() { return NULL; }
    stk::RunFuncType m_func;
};

void RunExample()
{
    using namespace stk;

    InitLEDs();

    // allocate scheduling kernel for THREADS_MAX threads (tasks) with Round-robin scheduling strategy
    static Kernel<KERNEL_DYNAMIC, THREADS_MAX, SwitchStrategyRoundRobin, PlatformDefault> kernel;

    // note: using ACCESS_PRIVILEGED as some MCUs may not allow writing to GPIO from a user thread, such as i.MX RT1050 (Arm Cortex-M7)
    static Thread<ACCESS_PRIVILEGED> task[THREADS_MAX];

    // init scheduling kernel
    kernel.Initialize();

    static bool toogle = false;
    while (true)
    {
        // init counter (or re-init in the next round)
        g_Counter = 0;

        // register threads (tasks)
        for (int32_t i = 0; i < THREADS_MAX; ++i)
        {
            // we shall see Green/Red LEDs switching on one by one in a loop
            task[i].m_func = (toogle ? IncrementWithCS : IncrementWithoutCS);

            // add task to the kernel
            kernel.AddTask(&task[i]);
        }

        // start scheduler (it will start threads added by AddTask), execution in main() 
        // will be blocked on this line until all tasks exit (complete)
        kernel.Start();

        // signal result
        Led::Set(Led::GREEN, g_Counter == COUNTER_EXPECT);
        Led::Set(Led::RED, g_Counter != COUNTER_EXPECT);

        // switch to the next round
        toogle = !toogle;
    }
}
