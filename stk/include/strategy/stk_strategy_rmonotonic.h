/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_STRATEGY_RM_H_
#define STK_STRATEGY_RM_H_

#include "stk_common.h"
#include "stk_strategy_rrobin.h"

namespace stk {

/*! \enum  EMonotonicSwitchStrategyType
    \brief Types for SwitchStrategyMonotonic.
*/
enum EMonotonicSwitchStrategyType
{
    MSS_TYPE_RATE,    //!< Rate-Monotonic (RM) type (smaller periodicity means higher priority, higher priority task is served first)
    MSS_TYPE_DEADLINE //!< Deadline-Monotonic (DM) type (shorter deadline means higher priority, higher priority task is served first)
};

/*! \class SwitchStrategyRoundRobin
    \brief Rate Monotonic Scheduling with RMUB and WCRT checks.

    Tasks are scheduled by fixed priority according to the selected monotonic policy (shorter execution time for RM,
    shorter deadline for DM). Provides methods to check CPU schedulability using RMUB or Worst-Case Response Time (WCRT).
*/
template <EMonotonicSwitchStrategyType _Type>
class SwitchStrategyMonotonic : public ITaskSwitchStrategy
{
public:
    enum { WEIGHT_API = 0 };

    void AddTask(IKernelTask *task)
    {
        if (m_tasks.IsEmpty())
        {
            m_tasks.LinkFront(task);
            return;
        }

        IKernelTask::ListEntryType *itr = m_tasks.GetFirst(), * const start = itr;

        while (true)
        {
            IKernelTask *cmp = (*itr);

            bool higher_priority;
            switch (_Type)
            {
            case MSS_TYPE_RATE:
                higher_priority = (task->GetHrtPeriodicity() < cmp->GetHrtPeriodicity());
                break;
            case MSS_TYPE_DEADLINE:
                higher_priority = (task->GetHrtDeadline() < cmp->GetHrtDeadline());
                break;
            default:
                STK_ASSERT(false);
                break;
            }

            if (higher_priority)
            {
                if (cmp == start)
                {
                    m_tasks.LinkFront(task);
                    break;
                }

                m_tasks.Link(task, cmp, cmp->GetPrev());
                break;
            }

            itr = itr->GetNext();

            // end of the list
            if (start == itr)
            {
                m_tasks.LinkBack(task);
                break;
            }
        }
    }

    void RemoveTask(IKernelTask *task) { m_tasks.Unlink(task); }

    IKernelTask *GetNext(IKernelTask *current) const
    {
        STK_ASSERT(!m_tasks.IsEmpty());

        IKernelTask::ListEntryType *itr = m_tasks.GetFirst();
        IKernelTask *highest_ready = nullptr;

        // highest priority = first in sorted list (shortest period)
        do
        {
            IKernelTask *task = (*itr);

            if (!task->IsSleeping())
            {
                highest_ready = task;
                break; // because list is sorted by priority
            }
        }
        while ((itr = itr->GetNext()) != m_tasks.GetFirst());

        // if no task ready -> idle (or return current)
        return (nullptr == highest_ready ? current : highest_ready);
    }

    IKernelTask *GetFirst() const
    {
        STK_ASSERT(m_tasks.GetSize() != 0);
        return (*m_tasks.GetFirst());
    }

    size_t GetSize() const { return m_tasks.GetSize(); }

    /*! \class TaskTiming
        \brief Period and execution time parameters used for WCRT analysis.

        Provides the minimal timing information required by CalculateWCRT(). Each entry corresponds
        to a single periodic task in the system, where:

          - \a periodicity  represents the execution time C of the task.
          - \a deadline     represents the task's deadline T.

        The task array must be ordered by priority before WCRT analysis is performed (see Rate-Monotonic, Deadline-Monotonic).
    */
    struct TaskTiming
    {
        uint32_t periodicity; //!< Execution time C of the task
        uint32_t deadline;    //!< Deadline T of the task
    };

    /*! \class TaskCpuLoad
        \brief Calculated CPU load of the task.
    */
    struct TaskCpuLoad
    {
        uint16_t task;  //!< CPU load of the task
        uint16_t total; //!< total CPU load reached by this task
    };

    /*! \class TaskInfo
        \brief Calculated task details (CPU load, WCRT).
    */
    struct TaskInfo
    {
        TaskCpuLoad cpu_load; //!< CPU load
        uint32_t    wcrt;     //!< WCRT of the tasks
    };

    /*! \class SchedulabilityCheckResult
        \brief The schedulability result with calculated WCRT of the tasks.

        Usage example:
        \code
        // for 3 tasks added to the kernel
        auto wcrt_sched = static_cast<SwitchStrategyRM *>(kernel.GetSwitchStrategy())->IsSchedulableWCRT<3>();
        STK_ASSERT(wcrt_sched == true);
        \endcode
    */

    /*! \class SchedulabilityCheckResult
        \brief Holds the result of a Worst-Case Response Time (WCRT) schedulability test.

        This structure stores:
          - the boolean schedulability result for the full task set
          - the computed WCRT value for each task

        The number of tasks is fixed at compile time through the template parameter \a _TaskCount.

        Usage example:
        \code
        // for 3 tasks added to the kernel
        auto wcrt_sched = static_cast<SwitchStrategyRM *>(kernel.GetSwitchStrategy())->IsSchedulableWCRT<3>();
        STK_ASSERT(wcrt_sched == true);
        \endcode
    */
    template <uint32_t _TaskCount>
    struct SchedulabilityCheckResult
    {
        bool     schedulable;      //!< schedulability test result
        TaskInfo info[_TaskCount]; //!< computed task info (CPU load, WCRT)

        /*! \brief  Check if tasks are schedulable.
            \return True schedulable, false otherwise.
         */
        operator bool() const { return schedulable; }
    };

    /*! \brief             Check if tasks can be scheduled using the Worst-Case Response Time (WCRT) analysis.
        \tparam _TaskCount Number of tasks to analyze. Must match the number of tasks added to the kernel.
        \return            A SchedulabilityCheckResult containing WCRT values for tasks and the schedulability result.
     */
    template <uint32_t _TaskCount>
    SchedulabilityCheckResult<_TaskCount> IsSchedulableWCRT()
    {
        STK_ASSERT(m_tasks.GetSize() == _TaskCount);

        SchedulabilityCheckResult<_TaskCount> ret;
        TaskTiming tasks[_TaskCount];

        // fill tasks timing
        IKernelTask *itr = (*m_tasks.GetFirst()), * const start = itr;
        uint32_t idx = 0;
        do
        {
            STK_ASSERT(idx < _TaskCount);

            tasks[idx] = { (uint32_t)itr->GetHrtPeriodicity(), (uint32_t)itr->GetHrtDeadline() };

            idx++;
            itr = (*itr->GetNext());
        }
        while (itr != start);
        STK_ASSERT(idx == _TaskCount);

        // calculate CPU load
        GetTaskCpuLoad(tasks, _TaskCount, ret.info);

        // run the WCRT schedulability analysis
        ret.schedulable = CalculateWCRT(tasks, _TaskCount, ret.info);

        return ret;
    }

    /*! \brief     Calculate the Worst-Case Response Time (WCRT) for a set of fixed-priority periodic tasks.

       This routine evaluates the schedulability of a fixed-priority monotonic task set using iterative
       Worst-Case Response Time analysis. Tasks are assumed to have:
         - fixed priorities (shorter period = higher priority)
         - periodic activation with deadline equal to their period
         - fully preemptive execution
         - no blocking from resource sharing

       For each task x, the WCRT value is computed iteratively according to:

            W = Cx + Σ ceil(W / Tj) * Cj

       where the summation runs over all tasks j with higher priority (as defined by the active monotonic policy).
       The iteration terminates when the response time converges or when W exceeds the task deadline.

       The input array must be ordered by increasing period (highest priority first). The computed WCRT
       values are written into the output array.

       \param[in]  tasks Pointer to an array of TaskTiming structures ordered by increasing period (highest priority first).
       \param[in]  count Number of tasks in the task set.
       \param[out] info  Pointer to an array of TaskInfo of size \a count that receives the computed WCRT values.

       \return     True if all tasks meet their deadlines (W ≤ deadline), false if any task violates its deadline.
     */
    static inline bool CalculateWCRT(const TaskTiming *tasks, const uint32_t count, TaskInfo *info)
    {
        bool schedulable = true;
        info[0].wcrt = tasks[0].periodicity;

        for (uint32_t t = 1; t < count; )
        {
            uint32_t w,
            Cx = tasks[t].periodicity,
            Tx = tasks[t].deadline,
            w0 = Cx;

        next_itr:

            w = Cx;
            for (uint32_t i = 0; i < t; ++i)
                w += idiv_ceil(w0, tasks[i].deadline) * tasks[i].periodicity;

            if ((w != w0) && (w <= Tx))
            {
                w0 = w;
                goto next_itr;
            }
            else
            {
                schedulable &= (w <= Tx);
                info[t++].wcrt = w;
            }
        }

        return schedulable;
    }

    /*! \brief      Calculate CPU load of the task set (in %).
        \param[in]  tasks Pointer to an array of TaskTiming structures.
        \param[in]  count Number of tasks in the task set.
        \param[out] info  Pointer to an array of TaskInfo of size \a count that receives the computed CPU load values.
    */
    static inline void GetTaskCpuLoad(const TaskTiming *tasks, const uint32_t count, TaskInfo *info)
    {
        uint16_t total = 0;

        for (uint32_t t = 0; t < count; ++t)
        {
            uint16_t task_load = (uint16_t)(tasks[t].periodicity * 100 / tasks[t].deadline);
            total += task_load;

            info[t].cpu_load.task  = task_load;
            info[t].cpu_load.total = total;
        }
    }

private:
    IKernelTask::ListHeadType m_tasks; //!< tasks for scheduling

    //! Integer division with ceiling, equivalent to: (int32_t)ceil((float)x / y).
    static __stk_forceinline int32_t idiv_ceil(uint32_t x, uint32_t y)
    {
        // http://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
        return x / y + (x % y > 0);
    }
};

/*! \typedef SwitchStrategyRM
    \brief   Rate-Monotonic (RM) switching strategy. A shortcut for SwitchStrategyMonotonic<MSS_TYPE_RATE>.
*/
typedef SwitchStrategyMonotonic<MSS_TYPE_RATE> SwitchStrategyRM;

/*! \typedef SwitchStrategyDM
    \brief   Deadline-Monotonic (RM) switching strategy. A shortcut for SwitchStrategyMonotonic<MSS_TYPE_DEADLINE>.
*/
typedef SwitchStrategyMonotonic<MSS_TYPE_DEADLINE> SwitchStrategyDM;

} // namespace stk

#endif /* STK_STRATEGY_RM_H_ */
