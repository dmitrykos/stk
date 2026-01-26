/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"

namespace stk {
namespace test {

// ============================================================================ //
// ================================ Sync ====================================== //
// ============================================================================ //

TEST_GROUP(Sync)
{
    void setup() {}
    void teardown() {}
};

TEST(Sync, CriticalSection)
{
    CHECK_EQUAL(0, test::g_CriticalSectionState);

    {
        sync::ScopedCriticalSection cs;

        CHECK_EQUAL(1, test::g_CriticalSectionState);
    }

    CHECK_EQUAL(0, test::g_CriticalSectionState);
}

} // namespace stk
} // namespace test
