/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
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
    \brief Inline function (function prefix).
*/
#ifdef __GNUC__
    #define __stk_forceinline __attribute__((always_inline)) inline
#elif defined(__ICCARM__) || defined(_MSC_VER)
    #define __stk_forceinline __forceinline
#else
    #define __stk_forceinline
#endif

/*! \def       __stk_aligned
    \brief     Align data structure to the x bytes (data instance prefix).
    \param[in] x: Alignment value in bytes.
*/
#ifdef __GNUC__
    #define __stk_aligned(x) __attribute__((aligned(x)))
#elif defined(__ICCARM__)
    #define __stk_aligned(x) __attribute__((aligned(x)))
#else
    #define __stk_aligned(x)
#endif

/*! \def   __stk_attr_naked
    \brief Instruct compiler that function does not have prologue and epilogue (function prefix).
*/
#ifdef __GNUC__
    #define __stk_attr_naked __attribute__((naked))
#elif defined(__ICCARM__)
    #define __stk_attr_naked __attribute__((naked))
#else
    #define __stk_attr_naked
#endif

/*! \def   __stk_attr_noreturn
    \brief Instruct compiler that function never returns (function prefix).
*/
#ifdef __GNUC__
    #define __stk_attr_noreturn __attribute__((__noreturn__))
#elif defined(__ICCARM__)
    #define __stk_attr_noreturn __attribute__((noreturn))
#else
    #define __stk_attr_noreturn
#endif

/*! \def   __stk_attr_unused
    \brief Instruct compiler that marked type or object may be unused.
*/
#ifdef __GNUC__
    #define __stk_attr_unused __attribute__((unused))
#elif defined(__ICCARM__)
    #define __stk_attr_unused __attribute__((unused))
#else
    #define __stk_attr_unused
#endif

/*! \def   __stk_attr_used
    \brief Instruct compiler that marked type or object is used.
*/
#ifdef __GNUC__
    #define __stk_attr_used __attribute__((used))
#elif defined(__ICCARM__)
    #define __stk_attr_used __attribute__((used))
#else
    #define __stk_attr_used
#endif

/*! \def   __stk_attr_noinline
    \brief Instruct compiler to not inline the function.
*/
#ifdef __GNUC__
    #define __stk_attr_noinline __attribute__((noinline))
#elif defined(__ICCARM__)
    #define __stk_attr_noinline __attribute__((noinline))
#else
    #define __stk_attr_noinline
#endif

/*! \def   __stk_unreachable
    \brief Instruct compiler that code below it is unreachable (in-code statement).
*/
#ifdef __GNUC__
    #define __stk_unreachable() __builtin_unreachable()
#else
    #define __stk_unreachable()
#endif

/*! \def   __stk_full_memfence
    \brief Full memory barrier.
*/
#ifdef __GNUC__
    #define __stk_full_memfence() __sync_synchronize()
#else
    #define __stk_full_memfence()
#endif

/*! \def   __stk_relax_cpu
    \note  Can be redefined by STK tests to intercept control inside the waiting loops in the Kernel.
    \brief Emits CPU relaxing instruction for usage inside a hot-spinning loop.
*/
#ifndef __stk_relax_cpu
#ifdef __GNUC__
    #if defined(__i386__) || defined(__x86_64__)
        #define __stk_relax_cpu() __builtin_ia32_pause()
    #else
        #define __stk_relax_cpu() __stk_full_memfence()
    #endif
#else
    #define __stk_relax_cpu()
#endif
#endif

/*! \def   STK_ASSERT
    \brief A shortcut to the assert() function. Can be overridden by the alternative _STK_ASSERT_FUNC
           if _STK_ASSERT_REDIRECT is defined.
*/
#ifdef _STK_ASSERT_REDIRECT
    extern void STK_ASSERT_IMPL(const char *, const char *, int32_t);
    #define STK_ASSERT(e) ((e) ? (void)0 : STK_ASSERT_IMPL(#e, __FILE__, __LINE__))
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

/*! \def   STK_STACK_MEMORY_FILLER
    \brief Stack memory filler (stack memory is filled with this value when initialized).
*/
#ifndef STK_STACK_MEMORY_FILLER
    #define STK_STACK_MEMORY_FILLER ((size_t)(sizeof(size_t) <= 4 ? 0xdeadbeef : 0xdeadbeefdeadbeef))
#endif

/*! \def   STK_STACK_SIZE_MIN
    \brief Minimal stack size (number of size_t).
    \see   TrapStackStackMemory
    \note  This size forms a minimal possible stack for the service traps of the scheduler. Depending
           on the CPU architecture and the number of supported CPU registers default minimal size may
           not be enough, for example: RISC-V, RV32I requires it to be at least 64, while RV32I + RVF
           doubles this to 128. STK_STACK_SIZE_MIN can be redefined to the desired value in
           stk_config.h file.
*/
#ifndef STK_STACK_SIZE_MIN
    #define STK_STACK_SIZE_MIN 32
#endif

/*! \def   STK_ALLOCATE_COUNT
    \brief Get count of objects to be allocated statically within the array.
    \note  Microsoft compiler does not support zero sized arrays unlike GCC or Clang, thus always allocate.
*/
#ifdef _MSC_VER
    #define STK_ALLOCATE_COUNT(MODE, FLAG, ONTRUE, ONFALSE) ((ONTRUE) > (ONFALSE) ? (ONTRUE) : (ONFALSE))
#else
    #define STK_ALLOCATE_COUNT(MODE, FLAG, ONTRUE, ONFALSE) ((MODE) & (FLAG) ? (ONTRUE) : (ONFALSE))
#endif

/*! \namespace stk
    \brief     Namespace of STK package.
 */
namespace stk {

/*! \namespace stk::util
    \brief     Namespace of the helper inventory.
 */
namespace util {}

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
