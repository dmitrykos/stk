/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_H_
#define STK_ARCH_H_

/*! \file  stk_arch.h
    \brief Contains platform implementations (IPlatform).
*/

#ifdef _STK_ARCH_ARM_CORTEX_M
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"
#define _STK_ARCH_DEFINED
#endif
#ifdef _STK_ARCH_RISC_V
#include "arch/risc-v/stk_arch_risc-v.h"
#define _STK_ARCH_DEFINED
#endif
#ifdef _STK_ARCH_X86_WIN32
#include "arch/x86/win32/stk_arch_x86-win32.h"
#define _STK_ARCH_DEFINED
#endif

#ifdef _STK_ARCH_DEFINED

namespace stk {

/*! \brief  Get thread-local storage (TLS) pointer.
    \return TLS pointer of _TyTls type.
*/
template <class _TyTls>
__stk_forceinline _TyTls *GetTlsPtr()
{
    return reinterpret_cast<_TyTls *>(GetTls());
}

/*! \brief     Set thread-local storage (TLS) pointer.
    \param[in] tp: TLS pointer of _TyTls type.
*/
template <class _TyTls>
__stk_forceinline void SetTlsPtr(const _TyTls *tp)
{
    SetTls(reinterpret_cast<uintptr_t>(tp));
}

/*! \class ScopedCriticalSection
    \brief Enters critical section and exits from it automatically within the scope of execution.

    Usage example:
    \code
    {
        ScopedCriticalSection __my_cs;
        
        // code below is free from race condition
    }
    \endcode
*/
struct ScopedCriticalSection
{
    ScopedCriticalSection() { stk::EnterCriticalSection(); }
    ~ScopedCriticalSection() { stk::ExitCriticalSection(); }
};

} // namespace stk

#endif // _STK_ARCH_UNDEFINED

#endif /* STK_ARCH_H_ */
