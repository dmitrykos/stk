/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stdio.h"
#include "perf.h"

Crc32Bench g_Bench[_STK_BENCH_TASK_MAX];

Crc32Bench::Crc32Bench()
{
    Initialize();
}

void Crc32Bench::Initialize()
{
    crc32_init(&m_crc);
    m_round = 0;
}

void Crc32Bench::Process()
{
    ++m_round;

    union
    {
        struct
        {
            uint64_t round;
            uint32_t crc;
        };

        uint8_t block[12];
    } u;

    u.round = m_round;
    u.crc   = m_crc.currentCrc;

    crc32_update(&m_crc, u.block, sizeof(u.block));
}

void Crc32Bench::ShowResults()
{
    for (int32_t i = 0; i < _STK_BENCH_TASK_MAX; ++i)
    {
        printf("task %d = %d\n", (int)i, (int)g_Bench[0].m_round);
    }
}

