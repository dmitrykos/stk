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

TestContext g_TestContext;

int main(int argc, char **argv)
{
	printf("STKTEST-START\n");

	int result = RUN_ALL_TESTS(argc, argv);

	printf("STKTEST-FAILED: %d\n", result);
}

/*! \fn    _STK_ASSERT_IMPL
    \brief Custom assertion handler which intercepts assertions from STK package.
*/
extern void _STK_ASSERT_IMPL(const char *message, const char *file, int32_t line)
{
	if (g_TestContext.IsExpectingAssert())
		throw TestAssertPassed();

	SimpleString what = "Assertion failed!\n";
	what += "\twhat: ";
	what += message;
	what += "\n\tfile: ";
	what += file;
	what += "\n\tline: ";
	what += StringFrom(line);

	CHECK_TEXT(false, what.asCharString());
}
