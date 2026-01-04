/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_ARM_CORTEX_M_H_
#define STK_ARCH_ARM_CORTEX_M_H_

#include "stk_common.h"

namespace stk {

/*! \class PlatformArmCortexM
    \brief Concrete implementation of IPlatform driver for the Arm Cortex-M0, M3, M4, M7 processors.
*/
class PlatformArmCortexM : public IPlatform
{
public:
    void Initialize(IEventHandler *event_handler, IKernelService *service, uint32_t resolution_us, Stack *exit_trap);
    void Start();
    void Stop();
    bool InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task);
    int32_t GetTickResolution() const;
    void SwitchToNext();
    void SleepTicks(uint32_t ticks);
    void ProcessTick();
    void ProcessHardFault();
    void SetEventOverrider(IEventOverrider *overrider);
    size_t GetCallerSP();
};

/*! \typedef PlatformDefault
    \brief   Default platform implementation.
*/
typedef PlatformArmCortexM PlatformDefault;

/*! \brief  Get thread-local storage (TLS).
    \return TLS value.
    \note   Using r9 register.
*/
__stk_forceinline uintptr_t GetTls()
{
    uintptr_t tp;
    __asm volatile("MOV %0, r9" : "=r"(tp) : /* input: none */ : /* clobbers: none */);
    return tp;
}

/*! \brief     Set thread-local storage (TLS).
    \param[in] tp: TLS value.
    \note      Using r9 register.
*/
__stk_forceinline void SetTls(uintptr_t tp)
{
    __asm volatile("MOV r9, %0" : /* output: none */ : "r"(tp) : /* clobbers: none */);
}

/*! \brief     Enter a critical section.
    \note      Use with care, critical section changes timing of tasks. Supports nesting.
*/
void EnterCriticalSection();

/*! \brief     Exit a critical section.
    \note      Must follow EnterCriticalSection().
*/
void ExitCriticalSection();

} // namespace stk

#endif /* STK_ARCH_ARM_CORTEX_M_H_ */
