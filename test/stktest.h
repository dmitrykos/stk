/*
 * stktest.h
 *
 *  Created on: 1 Nov 2022
 *      Author: Dmitry Kostjuchenko
 */

#ifndef STKTEST_H_
#define STKTEST_H_

#include <stdio.h>
#include <exception>

// lib: cpputest
#include <CppUTest/TestHarness.h>

// lib: stk
#define _STK_ARCH_X86_WIN32
#define _STK_UNDER_TEST
#include <stk.h>

/*! \class TestAssertPassed
    \brief Throwable class for catching assertions from _STK_ASSERT_IMPL().
*/
struct TestAssertPassed : public std::exception
{
    const char *what() const noexcept { return "STK test suite exception (TestAssertPassed) thrown!"; }
};

/*! \class TestContext
    \brief Common context for executed tests.
*/
class TestContext
{
public:
    TestContext() : m_expect_assert(false)
    { }

    void ExpectAssert(bool expect) { m_expect_assert = expect; }
    bool IsExpectingAssert() const { return m_expect_assert; }

private:
    static TestContext m_instance;
    bool m_expect_assert;
};

/*! \var   g_TestContext
    \brief Global instance of the test context.
*/
extern TestContext g_TestContext;

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
    int32_t           m_resolution;
    bool              m_started;
    stk::EAccessMode  m_access_mode;
    uint32_t          m_context_switch_nr;
};

/*! \class TaskMock
    \brief User task mock.
*/
template <stk::EAccessMode _AccessMode>
class TaskMock : public stk::Task<256, _AccessMode>
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