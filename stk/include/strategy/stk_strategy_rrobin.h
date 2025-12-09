/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_STRATEGY_RROBIN_H_
#define STK_STRATEGY_RROBIN_H_

#include "stk_common.h"

namespace stk {

/*! \class SwitchStrategyRoundRobin
    \brief Tasks switching strategy concrete implementation - Round-Robin.

    Round-Robin: all tasks are given an equal amount of processing time.
*/
class SwitchStrategyRoundRobin : public ITaskSwitchStrategy
{
public:
    enum { WEIGHT_API = 0 };

    void AddTask(IKernelTask *task) { m_tasks.LinkBack(task); }

    void RemoveTask(IKernelTask *task) { m_tasks.Unlink(task); }

    IKernelTask *GetNext(IKernelTask *current)
    {
        STK_ASSERT(m_tasks.GetSize() != 0);
        return (* current->GetNext());
    }

    IKernelTask *GetFirst()
    {
        STK_ASSERT(m_tasks.GetSize() != 0);
        return (* m_tasks.GetFirst());
    }

    size_t GetSize() const { return m_tasks.GetSize(); }

private:
    IKernelTask::ListHeadType m_tasks; //!< tasks for scheduling
};

} // namespace stk

#endif /* STK_STRATEGY_RROBIN_H_ */
