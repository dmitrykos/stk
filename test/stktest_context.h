/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STKTEST_CONTEX_H_
#define STKTEST_CONTEX_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <exception>

namespace stk {
namespace test {

/*! \class TestContext
    \brief Common context for a test suite.
*/
class TestContext
{
public:
    enum EConsts
    {
        SUCCESS_EXIT_CODE         = 0,//!< exit code for exit() to denote the success of the test
        DEFAULT_FAILURE_EXIT_CODE = 1 //!< default exit code for exit() to denote failure of the test
    };

    TestContext() : m_expect_assert(false)
    { }

    /*! \brief     Start expecting assertion for the test case.
        \param[in] expect: True to expect otherwise False.
    */
    void ExpectAssert(bool expect) { m_expect_assert = expect; }

    /*! \brief     Check if test case is expecting assertion.
    */
    bool IsExpectingAssert() const { return m_expect_assert; }

    /*! \brief     Show text string as prologue before tests start.
    */
    static inline void ShowTestSuitePrologue()
    {
        printf("STKTEST-START\n");
    }

    /*! \brief     Show text string as epilogue after tests end.
        \param[in] result: 0 if no tests are failed otherwise number of failed tests.
    */
    static inline void ShowTestSuiteEpilogue(int32_t result)
    {
        printf("STKTEST-RESULT: %d\n", (int)result);
    }

    /*! \brief     Exit test suite process forcibly.
        \param[in] result: 0 if no tests are failed otherwise number of failed tests.
    */
    static inline void ForceExitTestSuie(int32_t result)
    {
        ShowTestSuiteEpilogue(result);
        exit(result);
    }

private:
    static TestContext m_instance; //!< global instance of the TestContext
    bool m_expect_assert;          //!< assert expectation flag
};

/*! \var   g_TestContext
    \brief Global instance of the TestContext.
*/
extern TestContext g_TestContext;

/*! \def   STK_TEST_CHECK_EQUAL
    \brief Compare values for equality.
*/
#define STK_TEST_CHECK_EQUAL(expected, actual)\
    if ((expected) != (actual)) {\
        printf("STK_TEST_CHECK_EQUAL failed: file[%s] line[%d]\nexpected: %d\n  actual: %d", __FILE__, __LINE__, (int)expected, (int)actual);\
        exit(1);\
    }

/*! \def   STK_TEST_DECL_ASSERT
    \brief Declare assertion redirector in the source file.
*/
#define STK_TEST_DECL_ASSERT\
    extern void _STK_ASSERT_IMPL(const char *message, const char *file, int32_t line)\
    {\
        printf("_STK_ASSERT failed: file[%s] line[%d] message: %s", file, (int)line, message);\
        abort();\
    }

} // namespace test
} // namespace stk

#endif /* STKTEST_CONTEX_H_ */
