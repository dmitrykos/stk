/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STKTEST_H_
#define STKTEST_H_

#include <stdio.h>
#include <exception>

// lib: cpputest
#include <CppUTest/TestHarness.h>

// lib: stk
#define _STK_UNDER_TEST
#include <stk_config.h>
#include <stk.h>

#include "stktest_context.h"

namespace stk {

/*! \namespace stk::test
    \brief     Namespace of the test inventory.
 */
namespace test {

/*! \class TestAssertPassed
    \brief Throwable class for catching assertions from _STK_ASSERT_IMPL().
*/
struct TestAssertPassed : public std::exception
{
    const char *what() const noexcept { return "STK test suite exception (TestAssertPassed) thrown!"; }
};

/*! \class PlatformTestMock
    \brief IPlatform mock.
*/
class PlatformTestMock : public IPlatform
{
public:
    explicit PlatformTestMock()
    {
        m_event_handler          = NULL;
        m_started                = false;
        m_first_task_Start       = NULL;
        m_stack_InitStack        = NULL;
        m_stack_memory_InitStack = NULL;
        m_user_task_InitStack    = NULL;
        m_exit_trap              = NULL;
        m_fail_InitStack         = 0;
        m_resolution             = 0;
        m_access_mode            = ACCESS_USER;
        m_context_switch_nr      = 0;
    }

    virtual ~PlatformTestMock()
    { }

    void Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task, Stack *exit_trap)
    {
        m_event_handler    = event_handler;
        m_started          = true;
        m_resolution       = resolution_us;
        m_first_task_Start = first_task;
        m_exit_trap        = exit_trap;
    }

    void Stop()
    {
    	m_started = false;
    }

    bool InitStack(Stack *stack, IStackMemory *stack_memory, ITask *user_task)
    {
        if (m_fail_InitStack)
            return false;

        m_stack_InitStack        = stack;
        m_stack_memory_InitStack = stack_memory;
        m_user_task_InitStack    = user_task;

        stack->SP = (size_t)stack_memory->GetStack();
        return true;
    }

    void SwitchContext()
    {
        ++m_context_switch_nr;
    }

    int32_t GetTickResolution() const
    {
        return m_resolution;
    }

    void SetAccessMode(EAccessMode mode)
    {
        m_access_mode = mode;
    }

    IEventHandler *m_event_handler;
    IKernelTask   *m_first_task_Start;
    Stack         *m_stack_InitStack;
    IStackMemory  *m_stack_memory_InitStack;
    ITask         *m_user_task_InitStack;
    Stack         *m_exit_trap;
    bool           m_fail_InitStack;
    int32_t        m_resolution;
    bool           m_started;
    EAccessMode    m_access_mode;
    uint32_t       m_context_switch_nr;
};

/*! \class KernelServiceMock
    \brief IKernelService mock.
*/
class KernelServiceMock : public IKernelService
{
public:
    KernelServiceMock()
    {
        m_inc_ticks  = false;
        m_ticks      = 0;
        m_resolution = 0;
    }
    virtual ~KernelServiceMock()
    { }

    int64_t GetTicks() const
    {
        if (m_inc_ticks)
            const_cast<int64_t &>(m_ticks) = m_ticks + 1;

        return m_ticks;
    }

    int32_t GetTicksResolution() const
    {
        return m_resolution;
    }

    bool    m_inc_ticks;
    int64_t m_ticks;
    int32_t m_resolution;
};

/*! \class TaskMock
    \brief Task mock.
    \note  QEMU allocates small stack for the function, therefore stack size is limited to 16 for tests to pass (256 was causing a hard fault).
*/
template <EAccessMode _AccessMode>
class TaskMock : public Task<16, _AccessMode>
{
public:
    RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((TaskMock *)user_data)->RunInner();
    }

    void RunInner() {}
};

} // namespace test
} // namespace stk

#endif /* STKTEST_H_ */
