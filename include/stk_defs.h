/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_DEFS_H_
#define STK_DEFS_H_

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

// note: If missing, this header must be customized (get it in the root of the source folder) and
//       copied to the /include folder manually.
#include "stk_config.h"

#ifdef __GNUC__
	#define __stk_unreachable() __builtin_unreachable()
#endif

#ifdef __GNUC__
	#define __stk_attr_noreturn __attribute__((__noreturn__))
#elif defined(__ICCARM__)
	#define __stk_attr_noreturn __attribute__((noreturn))
#endif

#ifdef __GNUC__
	#define __stk_attr_naked __attribute__((naked))
#elif defined(__ICCARM__)
	#define __stk_attr_naked __attribute__((naked))
#endif

#ifdef __GNUC__
	#define __stk_aligned(x) __attribute__((aligned(x)))
#elif defined(__ICCARM__)
	#define __stk_aligned(x) __attribute__((aligned(x)))
#endif

#ifdef __GNUC__
	#define __stk_forceinline __attribute__((always_inline)) inline
#elif defined(__ICCARM__)
	#define __stk_forceinline __forceinline
#endif

template <class _To, class _From> static inline _To forced_cast(_From from)
{
    union { _From from; _To to; } cast;
    cast.from = from;
    return cast.to;
}

#endif /* STK_DEFS_H_ */
