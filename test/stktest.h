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
class PlatformTestMock : public stk::IPlatform
{
public:
    explicit PlatformTestMock()
    {
        m_event_handler       = NULL;
        m_started             = false;
        m_first_task_Start    = NULL;
        m_stack_InitStack     = NULL;
        m_user_task_InitStack = NULL;
        m_fail_InitStack      = 0;
        m_resolution          = 0;
        m_access_mode         = stk::ACCESS_USER;
        m_context_switch_nr   = 0;
    }
    virtual ~PlatformTestMock()
    { }
    void Start(IEventHandler *event_handler, uint32_t resolution_us, stk::IKernelTask *first_task)
    {
        m_event_handler    = event_handler;
        m_started          = true;
        m_resolution       = resolution_us;
        m_first_task_Start = first_task;
    }
    bool InitStack(stk::Stack *stack, stk::ITask *user_task)
    {
        if (m_fail_InitStack)
            return false;

        m_stack_InitStack     = stack;
        m_user_task_InitStack = user_task;

        stack->SP = (size_t)user_task->GetStack();
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
    void SetAccessMode(stk::EAccessMode mode)
    {
        m_access_mode = mode;
    }

    IEventHandler    *m_event_handler;
    stk::IKernelTask *m_first_task_Start;
    stk::Stack       *m_stack_InitStack;
    stk::ITask       *m_user_task_InitStack;
    bool              m_fail_InitStack;
    int32_t           m_resolution;
    bool              m_started;
    stk::EAccessMode  m_access_mode;
    uint32_t          m_context_switch_nr;
};

/*! \class TaskMock
    \brief User task mock.
    \note  QEMU allocates small stack for the function, therefore stack size is limited to 16 for tests to pass (256 was causing a hard fault).
*/
template <stk::EAccessMode _AccessMode>
class TaskMock : public stk::Task<16, _AccessMode>
{
public:
    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((TaskMock *)user_data)->RunInner();
    }

    void RunInner()
    { }
};

#endif /* STKTEST_H_ */
