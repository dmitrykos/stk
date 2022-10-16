/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_H_
#define STK_H_

#include "stk_common.h"
#include "stk_helper.h"
#include "stk_arch.h"

// Scheduling strategies:
#include "strategy/stk_strategy_rrobin.h"

namespace stk {

// Forward declarations:
template <uint32_t Size> class Kernel;

//! Kernel singleton;
extern IKernelService *g_Kernel;

/*! \class Kernel
    \brief Concrete implementation of the thread scheduling kernel.

    Usage example:
    \code
    static Kernel<10> kernel;
    static PlatformArmCortexM platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static Task<ACCESS_PRIVILEGED> task1;
    static Task<ACCESS_USER> task2;
    static Task<ACCESS_USER> task3;

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(1000);
    \endcode
*/
template <uint32_t Size>
class Kernel : public IKernel, public IKernelService, private IPlatform::IEventHandler
{
    /*! \class KernelTask
        \brief Concrete implementation of the kernel task.
    */
    class KernelTask : public IKernelTask
    {
        friend class Kernel;

    public:
        KernelTask()
        {
            m_user = NULL;
        }

        ITask *GetUserTask() { return m_user; }

        Stack *GetUserStack() { return &m_stack; }

        bool IsBusy() const { return (m_user != NULL); }

    private:
        Stack  m_stack;
        ITask *m_user;
    };

public:
    enum EConsts
    {
        TASKS_NUM_MAX = Size
    };

    void Initialize(IPlatform *platform, ITaskSwitchStrategy *switch_strategy)
    {
        assert(platform != NULL);
        assert(switch_strategy != NULL);

        // allow initialization only once
        assert(m_platform == NULL);
        if (m_platform != NULL)
            return;

        m_platform        = platform;
        m_switch_strategy = switch_strategy;
        m_task_now        = NULL;
        m_task_next       = NULL;
        m_ticks           = 0;

        g_Kernel = this;
    }

    void AddTask(ITask *user_task)
    {
        KernelTask *task = AllocateNewTask(user_task);
        assert(task != NULL);

        m_switch_strategy->AddTask(task);
    }

    void Start(uint32_t resolution_us)
    {
        assert(resolution_us != 0);
        assert(m_platform != NULL);

        m_task_now = (KernelTask *)m_switch_strategy->GetFirst();
        assert(m_task_now != NULL);
        if (m_task_now == NULL)
            return;

        m_platform->Start(this, resolution_us, m_task_now);
    }

    int64_t GetTicks() const { return m_ticks; }

    int32_t GetTicksResolution() const { return m_platform->GetSysTickResolution(); }

protected:
    typedef KernelTask TaskStorageT[TASKS_NUM_MAX];

    KernelTask *AllocateNewTask(ITask *user_task)
    {
        assert(user_task != NULL);

        // look for a free kernel task
        KernelTask *kernel_task = NULL;
        for (uint32_t i = 0; i < TASKS_NUM_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->IsBusy())
            {
                // avoid task collision
                assert(task->m_user != user_task);
                if (task->m_user == user_task)
                    return NULL;

                // avoid stack collision
                assert(task->m_user->GetStack() != user_task->GetStack());
                if (task->m_user->GetStack() == user_task->GetStack())
                    return NULL;
            }
            else
            if (kernel_task == NULL)
            {
                kernel_task = task;
            }
        }

        // if NULL - exceeded max supported kernel task count
        if (kernel_task == NULL)
        {
            assert(false);
            return NULL;
        }

        // init stack of the user task
        if (!m_platform->InitStack(&kernel_task->m_stack, user_task))
        {
            assert(false);
            return NULL;
        }

        // make kernel task busy with user task
        kernel_task->m_user = user_task;

        return kernel_task;
    }

    void OnStart()
    {
        m_platform->SetAccessMode(m_task_now->GetUserTask()->GetAccessMode());
    }

    void OnSysTick(Stack **idle, Stack **active)
    {
        KernelTask *now = m_task_now;
        KernelTask *next = (KernelTask *)m_switch_strategy->GetNext(now);

        ++m_ticks;

        if (next != now)
        {
            (*idle) = &now->m_stack;
            (*active) = &next->m_stack;

            m_task_now = next;

            UpdateAccessMode(now, next);

            m_platform->SwitchContext();
        }
    }

    void UpdateAccessMode(KernelTask *now, KernelTask *next)
    {
        EAccessMode next_access_mode = next->GetUserTask()->GetAccessMode();

        if (now->GetUserTask()->GetAccessMode() != next_access_mode)
            m_platform->SetAccessMode(next_access_mode);
    }

    IPlatform           *m_platform;        //!< platform driver
    ITaskSwitchStrategy *m_switch_strategy; //!< task switching strategy
    TaskStorageT         m_task_storage;    //!< task storage
    KernelTask          *m_task_now;        //!< current task task
    KernelTask          *m_task_next;       //!< next task for a context switch
    int64_t              m_ticks;           //!< CPU ticks elapsed
};

} // namespace stk

#endif /* STK_H_ */
