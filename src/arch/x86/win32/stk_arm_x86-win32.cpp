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

#include "arch/x86/win32/stk_arch_x86-win32.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setjmp.h>

using namespace stk;

//! Internal context.
static struct Context
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *first_stack, int32_t systick_resolution)
    {
        m_handler            = handler;
        m_stack_idle         = first_stack;
        m_stack_active       = first_stack;
        m_systick_resolution = systick_resolution;
    }

    IPlatform::IEventHandler *m_handler; //!< kernel event handler
    Stack *m_stack_idle;                 //!< idle task stack
    Stack *m_stack_active;               //!< active task stack
    int32_t m_systick_resolution;        //!< system tick resolution (microseconds)
    HANDLE m_timer_thread;
}
g_Context;

static DWORD WINAPI TimerThread(LPVOID lpParam)
{
	while (true)
	{
		g_Context.m_handler->OnSysTick(&g_Context.m_stack_idle, &g_Context.m_stack_active);
		Sleep(g_Context.m_systick_resolution / 1000);
	}

	return 0;
}

void PlatformX86Win32::Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task)
{
    g_Context.Initialize(event_handler, first_task->GetUserStack(), resolution_us);

    g_Context.m_timer_thread = CreateThread(NULL, 0, &TimerThread, NULL, 0, NULL);
}

bool PlatformX86Win32::InitStack(Stack *stack, ITask *user_task)
{
    return true;
}

void PlatformX86Win32::SwitchContext()
{

}

int32_t PlatformX86Win32::GetTickResolution() const
{
    return g_Context.m_systick_resolution;
}

void PlatformX86Win32::SetAccessMode(EAccessMode mode)
{

}

#endif // _STK_ARCH_X86_WIN32
