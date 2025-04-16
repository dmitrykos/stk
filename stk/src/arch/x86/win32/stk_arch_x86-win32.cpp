/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

// note: If missing, this header must be customized (get it in the root of the source folder) and
//       copied to the /include folder manually.
#include "stk_config.h"

#ifdef _STK_ARCH_X86_WIN32

#include "arch/stk_arch_common.h"
#include "arch/x86/win32/stk_arch_x86-win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <list>
#include <vector>

#ifndef WINAPI
#define WINAPI __stdcall
#endif

typedef UINT MMRESULT;
typedef MMRESULT (WINAPI * timeBeginPeriodF)(UINT uPeriod);
static timeBeginPeriodF timeBeginPeriod = NULL;

#define STK_X86_WIN32_CRITICAL_SECTION CRITICAL_SECTION
#define STK_X86_WIN32_CRITICAL_SECTION_INIT(SES) InitializeCriticalSection(SES)
#define STK_X86_WIN32_CRITICAL_SECTION_START(SES) EnterCriticalSection(SES)
#define STK_X86_WIN32_CRITICAL_SECTION_END(SES) LeaveCriticalSection(SES)
#define STK_X86_WIN32_MIN_RESOLUTION (1000)
#define STK_X86_WIN32_GET_SP(STACK) (STACK + 2) // +2 to overcome stack filler check inside Kernel (adjusting to +2 preserves 8-byte alignment)

using namespace stk;

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, int32_t resolution_us)
    {
        PlatformContext::Initialize(handler, exit_trap, resolution_us);

        m_winmm_dll    = NULL;
        m_timer_thread = NULL;
        m_started      = false;

        STK_X86_WIN32_CRITICAL_SECTION_INIT(&m_cs);

        LoadWindowsAPI();
    }
    ~Context()
    {
        UnloadWindowsAPI();
    }

    void LoadWindowsAPI()
    {
        HMODULE winmm = GetModuleHandleA("Winmm");
        if (winmm == NULL)
            m_winmm_dll = winmm = LoadLibraryA("Winmm.dll");
        assert(winmm != NULL);

        timeBeginPeriod = (timeBeginPeriodF)GetProcAddress(winmm, "timeBeginPeriod");
        assert(timeBeginPeriod != NULL);

        timeBeginPeriod(1);
    }

    void UnloadWindowsAPI()
    {
        if (m_winmm_dll != NULL)
        {
            FreeLibrary(m_winmm_dll);
            m_winmm_dll = NULL;
        }
    }

    struct TaskContext
    {
        TaskContext() : m_task(NULL), m_stack(NULL), m_thread(NULL), m_thread_id(0)
        { }

        void Initialize(ITask *task, Stack *stack)
        {
            m_task      = task;
            m_stack     = stack;
            m_thread    = NULL;
            m_thread_id = 0;

            InitThread();
        }

        void InitThread()
        {
            // simulate stack size limitation
            uint32_t stack_size = m_task->GetStackSize() * sizeof(size_t);

            m_thread = CreateThread(NULL, stack_size, &TaskThread, this, CREATE_SUSPENDED, &m_thread_id);
        }

        static DWORD WINAPI TaskThread(LPVOID param)
        {
            TaskContext *tctx = (TaskContext *)param;

            tctx->m_task->GetFunc()(tctx->m_task->GetFuncUserData());

            return 0;
        }

        ITask *m_task;      //!< user task
        Stack *m_stack;     //!< user tasks's stack
        HANDLE m_thread;    //!< task's thread handle
        DWORD  m_thread_id; //!< task's thread id
    };

    bool InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task);
    void ConfigureTime();
    void StartActiveTask();
    void CreateTimerThreadAndJoin();
    void Cleanup();
    void ProcessTick();
    void SwitchContext();
    void SwitchToNext();
    void SleepTicks(uint32_t ticks);
    void Stop();
    size_t GetCallerSP();

    HMODULE                        m_winmm_dll;     //!< Winmm.dll (loaded with LoadLibrary)
    HANDLE                         m_timer_thread;  //!< timer thread handle
    std::list<TaskContext *>       m_tasks;         //!< list of task internal contexts
    std::vector<HANDLE>            m_task_threads;  //!< task threads
    STK_X86_WIN32_CRITICAL_SECTION m_cs;            //!< critical session
    bool                           m_started;       //!< started state's flag
}
g_Context;

static IPlatform::IEventOverrider *g_Overrider = NULL;
static Stack                      *g_SleepTrap = NULL;
static Stack                      *g_ExitTrap  = NULL;

static DWORD WINAPI TimerThread(LPVOID param)
{
    (void)param;

    DWORD wait_ms = g_Context.m_tick_resolution / 1000;

    while (WaitForSingleObject(g_Context.m_timer_thread, wait_ms) == WAIT_TIMEOUT)
    {
        g_Context.ProcessTick();
    }

    return 0;
}

void Context::ConfigureTime()
{
    // Windows timers are jittery, so make resolution more coarse
    if (m_tick_resolution < STK_X86_WIN32_MIN_RESOLUTION)
        m_tick_resolution = STK_X86_WIN32_MIN_RESOLUTION;

    // increase precision of ticks to at least 1 ms (although Windows timers will still be quite coarse and have jitter of +1 ms)
    timeBeginPeriod(1);
}

void Context::StartActiveTask()
{
    STK_ASSERT(m_stack_active != NULL);
    TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);
    STK_ASSERT(active_task != NULL);

    ResumeThread(active_task->m_thread);
}

void Context::CreateTimerThreadAndJoin()
{
    m_started = true;

    m_handler->OnStart(&m_stack_active);

    StartActiveTask();

    // create tick thread with highest priority
    m_timer_thread = CreateThread(NULL, 0, &TimerThread, NULL, 0, NULL);
    SetThreadPriority(m_timer_thread, THREAD_PRIORITY_TIME_CRITICAL);

    while (!m_task_threads.empty())
    {
        DWORD result = WaitForMultipleObjects(m_task_threads.size(), m_task_threads.data(), FALSE, INFINITE);
        STK_ASSERT(result != WAIT_TIMEOUT);
        STK_ASSERT(result != WAIT_ABANDONED);
        STK_ASSERT(result != WAIT_FAILED);

        STK_X86_WIN32_CRITICAL_SECTION_START(&m_cs);

        uint32_t i = 0;
        for (std::vector<HANDLE>::iterator itr = m_task_threads.begin(); itr != m_task_threads.end(); ++itr)
        {
            if (result == (WAIT_OBJECT_0 + i))
            {
                TaskContext *exiting_task = NULL;
                for (std::list<TaskContext *>::iterator titr = m_tasks.begin(); titr != m_tasks.end(); ++titr)
                {
                    if ((*titr)->m_thread == (*itr))
                    {
                        exiting_task = (*titr);
                        break;
                    }
                }
                STK_ASSERT(exiting_task != NULL);

                m_handler->OnTaskExit(exiting_task->m_stack);

                m_task_threads.erase(itr);
                break;
            }

            ++i;
        }

        STK_X86_WIN32_CRITICAL_SECTION_END(&m_cs);
    }

    // join (never returns to the caller from here unless thread is terminated, see KERNEL_DYNAMIC)
    WaitForSingleObject(m_timer_thread, INFINITE);
}

void Context::Cleanup()
{
    // close thread handles of all tasks
    for (std::list<TaskContext *>::iterator itr = m_tasks.begin(); itr != m_tasks.end(); ++itr)
    {
        CloseHandle((*itr)->m_thread);
    }

    // close timer thread
    CloseHandle(m_timer_thread);
}

void Context::ProcessTick()
{
    STK_X86_WIN32_CRITICAL_SECTION_START(&m_cs);

    if (m_handler->OnTick(&m_stack_idle, &m_stack_active))
        g_Context.SwitchContext();

    STK_X86_WIN32_CRITICAL_SECTION_END(&m_cs);
}

void Context::SwitchContext()
{
    // suspend Idle thread
    if ((m_stack_idle != g_SleepTrap) && (m_stack_idle != g_ExitTrap))
    {
        TaskContext *idle_task = reinterpret_cast<TaskContext *>(m_stack_idle->SP);
        STK_ASSERT(idle_task != NULL);

        SuspendThread(idle_task->m_thread);
    }

    // resume Active thread
    if (m_stack_active == g_SleepTrap)
    {
        if ((g_Overrider == NULL) || !g_Overrider->OnSleep())
        {
            // pass
        }
    }
    else
    if (m_stack_active == g_ExitTrap)
    {
        // pass
    }
    else
    {
        TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);
        STK_ASSERT(active_task != NULL);

        ResumeThread(active_task->m_thread);
    }
}

size_t Context::GetCallerSP()
{
    TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);
    return (size_t)STK_X86_WIN32_GET_SP(active_task->m_task->GetStack());
}

void Context::SwitchToNext()
{
    m_handler->OnTaskSwitch(GetCallerSP());
}

void Context::SleepTicks(uint32_t ticks)
{
    m_handler->OnTaskSleep(GetCallerSP(), ticks);
}

void Context::Stop()
{
    TerminateThread(m_timer_thread, 0);

    m_started = false;
}

bool Context::InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    InitStackMemory(stack_memory);

    size_t *stack_top = STK_X86_WIN32_GET_SP(stack_memory->GetStack());

    switch (stack_type)
    {
    case STACK_USER_TASK: {
        TaskContext *ctx = (TaskContext *)stack_top;

        ctx->Initialize(user_task, stack);

        m_tasks.push_back(ctx);
        m_task_threads.push_back(ctx->m_thread);
        break; }

    case STACK_SLEEP_TRAP: {
        g_SleepTrap = stack;
        break; }

    case STACK_EXIT_TRAP: {
        g_ExitTrap = stack;
        break; }
    }

    stack->SP = reinterpret_cast<size_t>(stack_top);

    return true;
}

void PlatformX86Win32::Start(IEventHandler *event_handler, uint32_t resolution_us, Stack *exit_trap)
{
    g_Context.Initialize(event_handler, exit_trap, resolution_us);

    g_Context.ConfigureTime();
    g_Context.CreateTimerThreadAndJoin();
    g_Context.Cleanup();
}

void PlatformX86Win32::Stop()
{
    g_Context.Stop();
}

bool PlatformX86Win32::InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    return g_Context.InitStack(stack_type, stack, stack_memory, user_task);
}

int32_t PlatformX86Win32::GetTickResolution() const
{
    return g_Context.m_tick_resolution;
}

void PlatformX86Win32::SetAccessMode(EAccessMode mode)
{
    (void)mode;
}

void PlatformX86Win32::SwitchToNext()
{
    g_Context.SwitchToNext();
}

void PlatformX86Win32::SleepTicks(uint32_t ticks)
{
    g_Context.SleepTicks(ticks);
}

void PlatformX86Win32::ProcessTick()
{
    g_Context.ProcessTick();
}

void PlatformX86Win32::ProcessHardFault()
{
    if ((g_Overrider == NULL) || !g_Overrider->OnHardFault())
    {
        printf("failure: HardFault\n");
        exit(1);
    }
}

void PlatformX86Win32::SetEventOverrider(IEventOverrider *overrider)
{
    STK_ASSERT(!g_Context.m_started);
    g_Overrider = overrider;
}

size_t PlatformX86Win32::GetCallerSP()
{
    return g_Context.GetCallerSP();
}

#endif // _STK_ARCH_X86_WIN32
