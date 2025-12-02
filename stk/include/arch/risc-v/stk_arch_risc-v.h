/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_RISC_V_H_
#define STK_ARCH_RISC_V_H_

#include "stk_common.h"

namespace stk {

/*! \class PlatformRiscV
    \brief Concrete implementation of IPlatform driver for the Risc-V processors.
*/
class PlatformRiscV : public IPlatform
{
public:
    /*! \class IEventHandler
        \brief RISC-V specific event handler.
    */
    class ISpecificEventHandler
    {
    public:
        /*! \brief Called by ISR handler on IRQ_XXX (see encoding.h).
            \note if scheduler is not started and ecall is invoked then
        */
        virtual bool OnException(size_t cause) = 0;
    };

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

    void SetSpecificEventHandler(ISpecificEventHandler *handler);
};

/*! \typedef PlatformDefault
    \brief   Default platform implementation.
*/
typedef PlatformRiscV PlatformDefault;

/*! \brief  Get thread-local storage (TLS).
    \return TLS value.
    \note   tp register is an alias for x4
*/
__stk_forceinline uintptr_t GetTls()
{
    uintptr_t tp;
    __asm volatile("mv %0, tp" : "=r"(tp) : /* input: none */ : /* clobbers: none */);
    return tp;
}

/*! \brief     Set thread-local storage (TLS).
    \param[in] tp: TLS value.
    \note      tp register is an alias for x4
*/
__stk_forceinline void SetTls(uintptr_t tp)
{
    __asm volatile("mv tp, %0" : /* output: none */ : "r"(tp) : /* clobbers: none */);
}

/*! \brief     Enter to critical section.
    \note      Use with care, critical section changes timing of tasks. Supports nesting.
*/
void EnterCriticalSection();

/*! \brief     Exit from critical section.
    \note      Must follow EnterCriticalSection().
*/
void ExitCriticalSection();

} // namespace stk

/*! \def   _STK_SYSTEM_CLOCK_VAR
    \brief Definition of the system clock variable holding frequency of the CPU.
*/
#ifndef _STK_SYSTEM_CLOCK_VAR
    #define _STK_SYSTEM_CLOCK_VAR SystemCoreClock
#endif

/*! \def   _STK_SYSTEM_CLOCK_FREQUENCY
    \brief System clock frequency (Hz). Default: 1 MHz.
*/
#ifndef _STK_SYSTEM_CLOCK_FREQUENCY
    #define _STK_SYSTEM_CLOCK_FREQUENCY 1000000
#endif

/*! \var   SystemCoreClock
    \brief System clock frequency (Hz).
*/
extern "C" volatile uint32_t _STK_SYSTEM_CLOCK_VAR;

#endif /* STK_ARCH_RISC_V_H_ */
