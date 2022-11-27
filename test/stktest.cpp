/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"
#include <CppUTest/CommandLineTestRunner.h>

// Define TestContext instance.
TestContext g_TestContext;

/*! \fn    _STK_ASSERT_IMPL
    \brief Custom assertion handler which intercepts assertions from STK package.
*/
extern void _STK_ASSERT_IMPL(const char *message, const char *file, int32_t line)
{
	if (g_TestContext.IsExpectingAssert())
	{
    #if CPPUTEST_HAVE_EXCEPTIONS
		throw TestAssertPassed();
    #else
		return;
    #endif
	}

	SimpleString what = "Assertion failed!\n";
	what += "\twhat: ";
	what += message;
	what += "\n\tfile: ";
	what += file;
	what += "\n\tline: ";
	what += StringFrom(line);

	CHECK_TEXT(false, what.asCharString());
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    TestContext::ShowTestSuitePrologue();

    int32_t result = RUN_ALL_TESTS(argc, argv);

    TestContext::ShowTestSuiteEpilogue(result);

    return result;
}
