/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
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
    enum EConfig
    {
        WEIGHT_API      = 0, // strategy does not need Weight API of the kernel task
        SLEEP_EVENT_API = 1  // strategy needs OnTaskSleep/OnTaskWake events
    };

    SwitchStrategyRoundRobin() : m_tasks(), m_sleep(), m_next(nullptr)
    {}

    void AddTask(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(task->GetHead() == nullptr);

        // first added is the next
        if (GetSize() == 0)
            m_next = task;

        m_tasks.LinkBack(task);
    }

    void RemoveTask(IKernelTask *task)
    {
        STK_ASSERT(task != nullptr);
        STK_ASSERT(GetSize() != 0);
        STK_ASSERT((task->GetHead() == &m_tasks) || (task->GetHead() == &m_sleep));

        // unlink from tasks and update next
        if (task->GetHead() == &m_tasks)
        {
            if (task == m_next)
                m_next = (*task->GetNext());

            m_tasks.Unlink(task);
        }
        else
        // unlink from sleeping list if was sleeping
        if (task->GetHead() == &m_sleep)
        {
            m_sleep.Unlink(task);
        }

        if (GetSize() == 0)
            m_next = nullptr;
    }

    IKernelTask *GetNext(IKernelTask *current)
    {
        IKernelTask *ret = m_next;

        if (ret != nullptr)
            m_next = (*ret->GetNext());

        return ret;
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

        // update next
        if (m_tasks.GetSize() == 1)
            m_next = nullptr;
        else
        if (task == m_next)
            m_next = (*task->GetNext());

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

        // update next
        if (m_next == nullptr)
            m_next = task;
    }

protected:
    IKernelTask::ListHeadType m_tasks; //!< runnable tasks
    IKernelTask::ListHeadType m_sleep; //!< sleeping tasks
    mutable IKernelTask      *m_next;  //!< next task to schedule
};

/*! \typedef SwitchStrategyRR
    \brief   Shortcut for SwitchStrategyRoundRobin.
*/
typedef SwitchStrategyRoundRobin SwitchStrategyRR;

} // namespace stk

#endif /* STK_STRATEGY_RROBIN_H_ */
