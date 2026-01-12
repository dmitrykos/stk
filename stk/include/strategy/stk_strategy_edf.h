/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
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
    enum EConfig
    {
        WEIGHT_API      = 0, // strategy does not need Weight API of the kernel task
        SLEEP_EVENT_API = 1  // strategy needs OnTaskSleep/OnTaskWake events
    };

    void AddTask(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(task->GetHead() == nullptr);

        m_tasks.LinkBack(task);
    }

    void RemoveTask(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(GetSize() != 0);
        STK_ASSERT((task->GetHead() == &m_tasks) || (task->GetHead() == &m_sleep));

        // update next
        if (task->GetHead() == &m_tasks)
            m_tasks.Unlink(task);
        else
        // unlink from sleeping list if was sleeping
        if (task->GetHead() == &m_sleep)
            m_sleep.Unlink(task);
    }

    IKernelTask *GetNext(IKernelTask */*current*/)
    {
        // all tasks are sleeping
        if (m_tasks.IsEmpty())
            return NULL;

        IKernelTask *itr = (*m_tasks.GetFirst()), * const start = itr;
        IKernelTask *earliest = itr;

        do
        {
            if (itr->GetHrtRelativeDeadline() < earliest->GetHrtRelativeDeadline())
                earliest = itr;
        }
        while ((itr = (*itr->GetNext())) != start);

        return earliest;
    }

    IKernelTask *GetFirst() const
    {
        STK_ASSERT(GetSize() != 0);

        if (!m_tasks.IsEmpty())
            return (*m_tasks.GetFirst());
        else
            return (*m_sleep.GetFirst());
    }

    size_t GetSize() const
    {
        return m_tasks.GetSize() + m_sleep.GetSize();
    }

    void OnTaskSleep(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(task->IsSleeping());
        STK_ASSERT(task->GetHead() == &m_tasks);

        m_tasks.Unlink(task);
        m_sleep.LinkBack(task);
    }

    void OnTaskWake(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(!task->IsSleeping());
        STK_ASSERT(task->GetHead() == &m_sleep);

        m_sleep.Unlink(task);
        m_tasks.LinkBack(task);
    }

protected:
    IKernelTask::ListHeadType m_tasks; //!< runnable tasks
    IKernelTask::ListHeadType m_sleep; //!< sleeping tasks
};

} // namespace stk

#endif /* STK_STRATEGY_EDF_H_ */
