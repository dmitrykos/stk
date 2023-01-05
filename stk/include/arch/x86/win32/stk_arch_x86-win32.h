/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
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
    void Start(IEventHandler *event_handler, uint32_t resolution_us, Stack *exit_trap);
    void Stop();
    bool InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task);
    void SwitchContext();
    int32_t GetTickResolution() const;
    void SetAccessMode(EAccessMode mode);
    void SwitchToNext();
    void SleepTicks(uint32_t ticks);
    void HardFault();
    void SetEventOverrider(IEventOverrider *overrider);
    size_t GetCallerSP();
};

/*! \typedef PlatformDefault
    \brief   Default platform implementation.
*/
typedef PlatformX86Win32 PlatformDefault;

} // namespace stk

#endif /* STK_ARCH_X86_WIN32_H_ */
