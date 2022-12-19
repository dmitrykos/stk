/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
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
#include <list>
#include <assert.h>

typedef UINT MMRESULT;
typedef MMRESULT (* timeBeginPeriodF)(UINT uPeriod);
static timeBeginPeriodF timeBeginPeriod = NULL;

#define _STK_ARCH_X86_WIN32_MIN_RESOLUTION (10 * 1000)

using namespace stk;

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, Stack *first_stack, int32_t resolution_us)
    {
        PlatformContext::Initialize(handler, exit_trap, first_stack, resolution_us);

        m_winmm_dll    = NULL;
        m_timer_thread = NULL;

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
        TaskContext(ITask *task) : m_task(task), m_thread(NULL)
        { }

        ITask *m_task;   //!< user task
        HANDLE m_thread; //!< task's thread handle
    };

    void ConfigureTime();
    void CreateThreadsForTasks();
    void CreateTimerThreadAndJoin();

    HMODULE                m_winmm_dll;     //!< Winmm.dll (loaded with LoadLibrary)
    HANDLE                 m_timer_thread;  //!< timer thread handle
    std::list<TaskContext> m_tasks;         //!< list of task internal contexts
}
g_Context;

static DWORD WINAPI TimerThread(LPVOID param)
{
    (void)param;

    DWORD wait_ms = g_Context.m_tick_resolution / 1000;
    while (WaitForSingleObject(g_Context.m_timer_thread, wait_ms) == WAIT_TIMEOUT)
    {
        g_Context.m_handler->OnSysTick(&g_Context.m_stack_idle, &g_Context.m_stack_active);
    }

    return 0;
}

static DWORD WINAPI TaskThread(LPVOID param)
{
    Context::TaskContext *tctx = (Context::TaskContext *)param;

    tctx->m_task->GetFunc()(tctx->m_task->GetFuncUserData());

    return 0;
}

void Context::ConfigureTime()
{
    // Windows timers are jittery, so make resolution more coarse
    if (m_tick_resolution < _STK_ARCH_X86_WIN32_MIN_RESOLUTION)
        m_tick_resolution = _STK_ARCH_X86_WIN32_MIN_RESOLUTION;

    // increase precision of ticks to at least 1 ms (although Windows timers will still be quite coarse and have jitter of +1 ms)
    timeBeginPeriod(1);
}

void Context::CreateThreadsForTasks()
{
    std::list<Context::TaskContext>::iterator first = m_tasks.begin();

    for (std::list<Context::TaskContext>::iterator itr = first; itr != m_tasks.end(); ++itr)
    {
        Context::TaskContext *tctx = &(*itr);

        // simulate stack size limitation
        uint32_t stack_size = tctx->m_task->GetStackSize() * sizeof(size_t);

        tctx->m_thread = CreateThread(NULL, stack_size, &TaskThread, tctx, CREATE_SUSPENDED, NULL);
    }

    // start first task
    ResumeThread((*first).m_thread);
}

void Context::CreateTimerThreadAndJoin()
{
    // create tick thread with highest priority
    g_Context.m_timer_thread = CreateThread(NULL, 0, &TimerThread, NULL, 0, NULL);
    SetThreadPriority(g_Context.m_timer_thread, THREAD_PRIORITY_TIME_CRITICAL);

    // join (never returns to the caller from here unless thread is killed)
    WaitForSingleObject(g_Context.m_timer_thread, INFINITE);
}

void PlatformX86Win32::Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task, Stack *exit_trap)
{
    g_Context.Initialize(event_handler, exit_trap, first_task->GetUserStack(), resolution_us);

    g_Context.ConfigureTime();
    g_Context.CreateThreadsForTasks();
    g_Context.CreateTimerThreadAndJoin();
}

void PlatformX86Win32::Stop()
{
	TerminateThread(g_Context.m_timer_thread, 0);
}

bool PlatformX86Win32::InitStack(Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    g_Context.m_tasks.push_back(Context::TaskContext(user_task));

    stack->SP = reinterpret_cast<size_t>(&g_Context.m_tasks.back());

    return true;
}

void PlatformX86Win32::SwitchContext()
{
    Context::TaskContext *idle_task = reinterpret_cast<Context::TaskContext *>(g_Context.m_stack_idle->SP);
    Context::TaskContext *active_task = reinterpret_cast<Context::TaskContext *>(g_Context.m_stack_active->SP);

    SuspendThread(idle_task->m_thread);
    ResumeThread(active_task->m_thread);
}

int32_t PlatformX86Win32::GetTickResolution() const
{
    return g_Context.m_tick_resolution;
}

void PlatformX86Win32::SetAccessMode(EAccessMode mode)
{
    (void)mode;
}

#endif // _STK_ARCH_X86_WIN32
