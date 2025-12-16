/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_STRATEGY_EDF_H_
#define STK_STRATEGY_EDF_H_

#include "stk_common.h"

namespace stk {

/*! \class SwitchStrategyEDF
    \brief Earliest Deadline First (EDF) scheduling strategy.

    Dynamic-priority scheduling: the task with the shortest relative deadline is always selected for execution.
*/
class SwitchStrategyEDF : public ITaskSwitchStrategy
{
public:
    enum { WEIGHT_API = 0 };

    void AddTask(IKernelTask *task) { m_tasks.LinkBack(task); }

    void RemoveTask(IKernelTask *task) { m_tasks.Unlink(task); }

    IKernelTask *GetNext(IKernelTask *current) const
    {
        STK_ASSERT(current != nullptr);
        STK_ASSERT(current->GetHead() == &m_tasks);
        STK_ASSERT(!m_tasks.IsEmpty());

        IKernelTask *itr = (*m_tasks.GetFirst()), * const start = itr;
        IKernelTask *earliest = nullptr;

        do
        {
            if (!itr->IsSleeping())
            {
                if ((earliest == nullptr) ||
                    (itr->GetHrtRelativeDeadline() < earliest->GetHrtRelativeDeadline()))
                {
                    earliest = itr;
                }
            }
        }
        while ((itr = (*itr->GetNext())) != start);

        // if no task is ready, stay on current
        return (earliest != nullptr ? earliest : current);
    }

    IKernelTask *GetFirst() const
    {
        STK_ASSERT(!m_tasks.IsEmpty());

        return (*m_tasks.GetFirst());
    }

    size_t GetSize() const { return m_tasks.GetSize(); }

protected:
    IKernelTask::ListHeadType m_tasks; //!< tasks for scheduling
};

} // namespace stk

#endif /* STK_STRATEGY_EDF_H_ */
