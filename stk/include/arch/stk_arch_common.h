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
        \param[in] idle_stack: Idle stack (if different from first_stack then this stack will be saved on scheduler start).
        \param[in] first_stack: First task's stack.
        \param[in] tick_resolution: Tick resolution in microseconds (for example 1000 equals to 1 millisecond resolution).
    */
    virtual void Initialize(IPlatform::IEventHandler *handler, Stack *idle_stack, Stack *first_stack, int32_t tick_resolution)
    {
        m_handler         = handler;
        m_stack_idle      = (idle_stack != NULL ? idle_stack : first_stack);
        m_stack_active    = first_stack;
        m_tick_resolution = tick_resolution;
    }

    IPlatform::IEventHandler *m_handler; //!< kernel event handler
    Stack  *m_stack_idle;                //!< idle task stack
    Stack  *m_stack_active;              //!< active task stack
    int32_t m_tick_resolution;           //!< system tick resolution (microseconds)
};

} // namespace stk

#endif /* STK_ARCH_COMMON_H_ */
