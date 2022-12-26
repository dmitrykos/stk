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
#include <assert.h>
#include <list>
#include <vector>

typedef UINT MMRESULT;
typedef MMRESULT (* timeBeginPeriodF)(UINT uPeriod);
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
    void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, Stack *first_stack, int32_t resolution_us)
    {
        PlatformContext::Initialize(handler, exit_trap, first_stack, resolution_us);

        m_winmm_dll     = NULL;
        m_timer_thread  = NULL;

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
        TaskContext() : m_task(NULL), m_thread(NULL), m_thread_id(0)
        { }

        void Initialize(ITask *task)
        {
            m_task      = task;
            m_thread    = NULL;
            m_thread_id = 0;
        }

        ITask *m_task;      //!< user task
        HANDLE m_thread;    //!< task's thread handle
        DWORD  m_thread_id; //!< task's thread id
    };

    void ConfigureTime();
    void CreateThreadsForTasks();
    void CreateTimerThreadAndJoin();
    void Cleanup();
    void SysTick();
    void SwitchContext();
    void SwitchToNext();
    void SleepTicks(uint32_t ticks);

    HMODULE                        m_winmm_dll;     //!< Winmm.dll (loaded with LoadLibrary)
    HANDLE                         m_timer_thread;  //!< timer thread handle
    std::list<TaskContext *>       m_tasks;         //!< list of task internal contexts
    std::vector<HANDLE>            m_task_threads;  //!< task threads
    STK_X86_WIN32_CRITICAL_SECTION m_cs;            //!< critical session
}
g_Context;

static DWORD WINAPI TimerThread(LPVOID param)
{
    (void)param;

    DWORD wait_ms = g_Context.m_tick_resolution / 1000;

    while (WaitForSingleObject(g_Context.m_timer_thread, wait_ms) == WAIT_TIMEOUT)
    {
        g_Context.SysTick();
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
    if (m_tick_resolution < STK_X86_WIN32_MIN_RESOLUTION)
        m_tick_resolution = STK_X86_WIN32_MIN_RESOLUTION;

    // increase precision of ticks to at least 1 ms (although Windows timers will still be quite coarse and have jitter of +1 ms)
    timeBeginPeriod(1);
}

void Context::CreateThreadsForTasks()
{
    std::list<TaskContext *>::iterator first = m_tasks.begin();

    for (std::list<TaskContext *>::iterator itr = first; itr != m_tasks.end(); ++itr)
    {
        TaskContext *tctx = (*itr);

        // simulate stack size limitation
        uint32_t stack_size = tctx->m_task->GetStackSize() * sizeof(size_t);

        tctx->m_thread = CreateThread(NULL, stack_size, &TaskThread, tctx, CREATE_SUSPENDED, &tctx->m_thread_id);
        m_task_threads.push_back(tctx->m_thread);
    }

    // start first task
    ResumeThread((*first)->m_thread);
}

static void OnTaskExit()
{
    g_Context.m_handler->OnTaskExit(g_Context.m_stack_active);
}

void Context::CreateTimerThreadAndJoin()
{
    // create tick thread with highest priority
    g_Context.m_timer_thread = CreateThread(NULL, 0, &TimerThread, NULL, 0, NULL);
    SetThreadPriority(g_Context.m_timer_thread, THREAD_PRIORITY_TIME_CRITICAL);

    while (!m_task_threads.empty())
    {
        DWORD result = WaitForMultipleObjects(m_task_threads.size(), m_task_threads.data(), FALSE, INFINITE);
        STK_ASSERT(result != WAIT_TIMEOUT);
        STK_ASSERT(result != WAIT_ABANDONED);
        STK_ASSERT(result != WAIT_FAILED);

        STK_X86_WIN32_CRITICAL_SECTION_START(&g_Context.m_cs);

        uint32_t i = 0;
        for (std::vector<HANDLE>::iterator itr = m_task_threads.begin(); itr != m_task_threads.end(); ++itr)
        {
            if (result == (WAIT_OBJECT_0 + i))
            {
                TaskContext *task_ctx = reinterpret_cast<TaskContext *>(g_Context.m_stack_active->SP);
                STK_ASSERT(task_ctx->m_thread == (*itr));

                OnTaskExit();

                m_task_threads.erase(itr);
                break;
            }

            ++i;
        }

        STK_X86_WIN32_CRITICAL_SECTION_END(&g_Context.m_cs);
    }

    // join (never returns to the caller from here unless thread is terminated, see KERNEL_DYNAMIC)
    WaitForSingleObject(g_Context.m_timer_thread, INFINITE);
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

void Context::SysTick()
{
    STK_X86_WIN32_CRITICAL_SECTION_START(&m_cs);

    m_handler->OnSysTick(&m_stack_idle, &m_stack_active);

    STK_X86_WIN32_CRITICAL_SECTION_END(&m_cs);
}

void Context::SwitchContext()
{
    TaskContext *idle_task = reinterpret_cast<TaskContext *>(m_stack_idle->SP);
    TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);

    if (idle_task != NULL)
        SuspendThread(idle_task->m_thread);

    ResumeThread(active_task->m_thread);
}

void Context::SwitchToNext()
{
    TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);
    size_t caller_SP = (size_t)STK_X86_WIN32_GET_SP(active_task->m_task->GetStack());

    m_handler->OnTaskSwitch(caller_SP);
}

void Context::SleepTicks(uint32_t ticks)
{
    TaskContext *active_task = reinterpret_cast<TaskContext *>(m_stack_active->SP);
    size_t caller_SP = (size_t)STK_X86_WIN32_GET_SP(active_task->m_task->GetStack());

    m_handler->OnTaskSleep(caller_SP, ticks);
}

void PlatformX86Win32::Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task, Stack *exit_trap)
{
    g_Context.Initialize(event_handler, exit_trap, first_task->GetUserStack(), resolution_us);

    g_Context.ConfigureTime();
    g_Context.CreateThreadsForTasks();
    g_Context.CreateTimerThreadAndJoin();
    g_Context.Cleanup();
}

void PlatformX86Win32::Stop()
{
	TerminateThread(g_Context.m_timer_thread, 0);
}

bool PlatformX86Win32::InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    g_Context.InitStackMemory(stack_memory);

    size_t *stack_top = STK_X86_WIN32_GET_SP(stack_memory->GetStack());

    if (stack_type == STACK_USER_TASK)
    {
        Context::TaskContext *ctx = (Context::TaskContext *)stack_top;

        ctx->Initialize(user_task);

        g_Context.m_tasks.push_back(ctx);
    }

    stack->SP = reinterpret_cast<size_t>(stack_top);

    return true;
}

void PlatformX86Win32::SwitchContext()
{
    g_Context.SwitchContext();
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

#endif // _STK_ARCH_X86_WIN32
