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

/*! \file  stk_defs.h
    \brief Contains compiler low-level definitions.
*/

/*! \def   __stk_forceinline
    \brief Inlines function (function prefix).
*/
#ifdef __GNUC__
    #define __stk_forceinline __attribute__((always_inline)) inline
#elif defined(__ICCARM__)
    #define __stk_forceinline __forceinline
#else
    #define __stk_forceinline
#endif

/*! \def   __stk_aligned
    \param[in] x: Alignment value in bytes.
    \brief Aligns data structure to the x bytes (data instance prefix).
*/
#ifdef __GNUC__
    #define __stk_aligned(x) __attribute__((aligned(x)))
#elif defined(__ICCARM__)
    #define __stk_aligned(x) __attribute__((aligned(x)))
#else
    #define __stk_aligned(x)
#endif

/*! \def   __stk_attr_naked
    \brief Instructs compiler that function does not have prologue and epilogue (function prefix).
*/
#ifdef __GNUC__
    #define __stk_attr_naked __attribute__((naked))
#elif defined(__ICCARM__)
    #define __stk_attr_naked __attribute__((naked))
#else
    #define __stk_attr_naked
#endif

/*! \def   __stk_attr_noreturn
    \brief Instructs compiler that function never returns (function prefix).
*/
#ifdef __GNUC__
    #define __stk_attr_noreturn __attribute__((__noreturn__))
#elif defined(__ICCARM__)
    #define __stk_attr_noreturn __attribute__((noreturn))
#else
    #define __stk_attr_noreturn
#endif

/*! \def   __stk_attr_unused
    \brief Instructs compiler that marked type or object may be unused.
*/
#ifdef __GNUC__
    #define __stk_attr_unused __attribute__((unused))
#elif defined(__ICCARM__)
    #define __stk_attr_unused __attribute__((unused))
#else
    #define __stk_attr_unused
#endif

/*! \def   __stk_unreachable
    \brief Instructs compiler that code below it is unreachable (in-code statement).
*/
#ifdef __GNUC__
    #define __stk_unreachable() __builtin_unreachable()
#else
    #define __stk_unreachable()
#endif


/*! \def   ____stk_relax_cpu
    \brief Emits CPU relaxing instruction for usage inside a hot-spinning loop.
*/
#ifdef __GNUC__
    #if defined(__i386__) || defined(__x86_64__)
        #define __stk_relax_cpu() __builtin_ia32_pause()
    #else
        #define __stk_relax_cpu() __sync_synchronize()
    #endif
#else
    #define __stk_relax_cpu()
#endif

/*! \def   stk_assert
    \brief A shortcut to the assert() function. Can be overridden by the alternative _STK_ASSERT_FUNC if _STK_ASSERT_REDIRECT is defined.
*/
#ifdef _STK_ASSERT_REDIRECT
    extern void _STK_ASSERT_IMPL(const char *, const char *, int32_t);
    #define STK_ASSERT(e) ((e) ? (void)0 : _STK_ASSERT_IMPL(#e, __FILE__, __LINE__))
#else
    #include <assert.h>
    #define STK_ASSERT(e) assert(e)
#endif

/*! \def   STK_STATIC_ASSERT_N
    \brief Complie-time assert with user-defined name.
*/
#define STK_STATIC_ASSERT_N(NAME, X) typedef char __stk_static_assert_##NAME[(X) ? 1 : -1] __stk_attr_unused

/*! \def   STK_STATIC_ASSERT
    \brief Complie-time assert.
*/
#define STK_STATIC_ASSERT(X) STK_STATIC_ASSERT_N(_, X)

/*! \namespace stk
    \brief     Namespace of STK package.
 */
namespace stk {

/*! \namespace platform
    \brief     Namespace for platform-specific inventory.
 */
namespace platform { }

/*! \fn    forced_cast
    \brief Force-cast value of one type to another. Overcomes compiler error or warning when trying
           to cast in normal way.
*/
template <class _To, class _From>
static __stk_forceinline _To forced_cast(const _From &from)
{
    union { _From from; _To to; } cast;
    cast.from = from;
    return cast.to;
}

} // namespace stk

#endif /* STK_DEFS_H_ */
