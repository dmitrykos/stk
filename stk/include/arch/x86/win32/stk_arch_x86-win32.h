/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_X86_WIN32_H_
#define STK_ARCH_X86_WIN32_H_

#include "stk_common.h"

namespace stk {

/*! \class PlatformX86Win32
    \brief Concrete implementation of IPlatform driver for the x86 Win32 platform.
    \note  Implemented for simulation purpose on Windows platform.
*/
class PlatformX86Win32 : public IPlatform
{
public:
    void Initialize(IEventHandler *event_handler, IKernelService *service, uint32_t resolution_us, Stack *exit_trap);
    void Start();
    void Stop();
    bool InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task);
    int32_t GetTickResolution() const;
    void SwitchToNext();
    void SleepTicks(Timeout ticks);
    IWaitObject *StartWaiting(ISyncObject *sync_obj, IMutex *mutex, Timeout timeout);
    void ProcessTick();
    void ProcessHardFault();
    void SetEventOverrider(IEventOverrider *overrider);
    size_t GetCallerSP() const;
    TId GetTid() const;
};

/*! \typedef PlatformDefault
    \brief   Default platform implementation.
*/
typedef PlatformX86Win32 PlatformDefault;

/*! \brief  Get thread-local storage (TLS).
    \return TLS value.
*/
uintptr_t GetTls();

/*! \brief     Set thread-local storage (TLS).
    \param[in] tp: TLS value.
*/
void SetTls(uintptr_t tp);

} // namespace stk

#endif /* STK_ARCH_X86_WIN32_H_ */
