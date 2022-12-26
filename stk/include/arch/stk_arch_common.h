/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_COMMON_H_
#define STK_ARCH_COMMON_H_

/*! \file  stk_arch_common.h
    \brief Contains common inventory for platform implementation.
*/

#include "stk_common.h"

namespace stk {

/*! \class PlatformContext
    \brief Base platform context for all platform implementations.
*/
class PlatformContext
{
public:
    /*! \brief     Initialize context.
        \param[in] handler: Event handler.
        \param[in] exit_trap: Exit trap's stack.
        \param[in] first_stack: First task's stack.
        \param[in] resolution_us: Tick resolution in microseconds (for example 1000 equals to 1 millisecond resolution).
    */
    virtual void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, Stack *first_stack, int32_t resolution_us)
    {
        m_handler         = handler;
        m_stack_idle      = exit_trap;
        m_stack_active    = first_stack;
        m_tick_resolution = resolution_us;
    }

    /*! \brief     Initialize stack memory by filling it with STK_STACK_MEMORY_FILLER.
        \note      Returned pointer is for a stack growing from top to down.
        \param[in] memory: Stack memory to initialize.
        \param[in] limit: Initialization depth limit.
        \return    Pointer to initialized stack memory.
    */
    static inline size_t *InitStackMemory(IStackMemory *memory, int32_t limit = 0)
    {
        int32_t stack_size = memory->GetStackSize();
        size_t *stack_top = memory->GetStack() + stack_size;

        STK_ASSERT(stack_size >= STACK_SIZE_MIN);
        STK_ASSERT(stack_size > limit);

        // initialization of the stack memory satisfies stack integrity check in Kernel::StateSwitch
        for (int32_t i = -stack_size; i <= -(limit + 1); ++i)
        {
            stack_top[i] = STK_STACK_MEMORY_FILLER;
        }

        return stack_top;
    }

    IPlatform::IEventHandler *m_handler;         //!< kernel event handler
    Stack                    *m_stack_idle;      //!< idle task stack
    Stack                    *m_stack_active;    //!< active task stack
    int32_t                   m_tick_resolution; //!< system tick resolution (microseconds)
};

} // namespace stk

#endif /* STK_ARCH_COMMON_H_ */
