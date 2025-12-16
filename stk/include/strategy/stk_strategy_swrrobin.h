/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_STRATEGY_SWRROBIN_H_
#define STK_STRATEGY_SWRROBIN_H_

#include "stk_common.h"

namespace stk {

/*! \class SwitchStrategySmoothWeightedRoundRobin
    \brief Tasks switching strategy - Smooth Weighted Round-Robin (SWRR).

    Smooth WRR distributes CPU time proportionally to weights, but avoids bursts
    by smoothing the task execution over the time. It is an improved variant of
    standard WRR: https://en.wikipedia.org/wiki/Weighted_round_robin

    Algorithm:

    Smooth Weighted Round-Robin (SWRR) uses a dynamic value called "current weight"
    to determine which task should be selected next. Each scheduling step increases
    the current weight of every task by its static weight, and the task with the
    highest current weight gets CPU time. After selection, the total weight of all
    tasks is subtracted from its current weight.

    Weight provided by GetWeight() defines how much CPU time the task should receive
    relative to other tasks participating in scheduling. A higher weight means a
    larger expected share of CPU time. Static weight is configured when a task is
    added to the scheduler.

    The dynamic counter provided by GetCurrentWeight() is modified by the scheduling
    strategy at runtime. During Smooth Weighted Round-Robin scheduling:
       - Every tick: current_weight += weight
       - When selected to run: current_weight -= total_weight_sum

    The task with the highest current weight is selected for execution.
    Over time, this produces fair proportional CPU distribution while
    avoiding long bursts of consecutive execution.
*/
class SwitchStrategySmoothWeightedRoundRobin : public ITaskSwitchStrategy
{
public:
    enum { WEIGHT_API = 1 };

    void AddTask(IKernelTask *task)
    {
        int32_t weight = task->GetWeight();
        STK_ASSERT((weight > 0) && (weight <= 0x7FFFFF)); // must not be negative, max 24-bit number

        task->SetCurrentWeight(0);

        m_tasks.LinkBack(task);
        m_total_weight += weight;
    }

    void RemoveTask(IKernelTask *task)
    {
        m_total_weight -= task->GetWeight();
        m_tasks.Unlink(task);
    }

    IKernelTask *GetNext(IKernelTask */*current*/) const
    {
        STK_ASSERT(!m_tasks.IsEmpty());

        IKernelTask *selected = nullptr;
        int32_t max_weight = -1;
        IKernelTask *itr = (*m_tasks.GetFirst()), * const start = itr;

        do
        {
            itr->SetCurrentWeight(itr->GetCurrentWeight() + itr->GetWeight());

            int32_t candidate_weight = itr->GetCurrentWeight();
            if (candidate_weight > max_weight)
            {
                max_weight = candidate_weight;
                selected = itr;
            }
        }
        while ((itr = (*itr->GetNext())) != start);
        STK_ASSERT(selected != nullptr);

        selected->SetCurrentWeight(max_weight - m_total_weight);

        return selected;
    }

    IKernelTask *GetFirst() const
    {
        STK_ASSERT(!m_tasks.IsEmpty());

        return (*m_tasks.GetFirst());
    }

    size_t GetSize() const { return m_tasks.GetSize(); }

private:
    IKernelTask::ListHeadType m_tasks;            //!< tasks for scheduling
    int32_t                   m_total_weight = 0; //!< sum of all task weights
};

/*! \typedef SwitchStrategySWRR
    \brief   Shortcut for SwitchStrategySmoothWeightedRoundRobin.
*/
typedef SwitchStrategySmoothWeightedRoundRobin SwitchStrategySWRR;

} // namespace stk

#endif /* STK_STRATEGY_SWRROBIN_H_ */
