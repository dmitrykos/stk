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

#include "stk_helper.h"
#include "stk_arch.h"

/*! \file  stk.h
    \brief Contains core implementation (Kernel) of the task scheduler.
*/

// Scheduling strategies:
#include "strategy/stk_strategy_rrobin.h"

namespace stk {

/*! \def   g_KernelService
    \brief Pointer to the IKernelService instance which becomes available when scheduling
           is started with Kernel::Start().
    \note  Singleton design pattern.
*/
#define g_KernelService stk::Singleton<stk::IKernelService *>::Get()

/*! \class Kernel
    \brief Concrete implementation of the thread scheduling kernel.
    \note  Kernel expects at least 1 task, e.g. Kernel<N> where N != 0.

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
template <uint32_t _Size>
class Kernel : public IKernel, private IPlatform::IEventHandler
{
    /*! \class KernelTask
        \brief Concrete implementation of the IKernelTask interface.
    */
    class KernelTask : public IKernelTask
    {
        friend class Kernel;

    public:
        /*! \brief Default initializer.
        */
        explicit KernelTask() : m_user(NULL)
        { }

        ITask *GetUserTask() { return m_user; }

        Stack *GetUserStack() { return &m_stack; }

        bool IsBusy() const { return (m_user != NULL); }

    private:
        ITask *m_user;  //!< user task
        Stack  m_stack; //!< stack descriptor
    };

    /*! \class KernelService
        \brief Concrete implementation of the IKernelService interface.
    */
    class KernelService : public IKernelService
    {
        friend class Kernel;

        /*! \class SingletonBinder
            \brief Exposes IKernelService instance through the Singleton<IKernelService *>::Get().
                   Acts as a singleton instance binder at run-time. IKernelService instance becomes
                   available before user tasks are started.
            \note  Singleton design pattern (see Singleton).
        */
        class SingletonBinder : private Singleton<IKernelService *>
        {
            //! \note Only Kernel's internal environment has access to the SingletonBinder::Bind() function.
            template <uint32_t _Size0> friend class Kernel;
        };

    public:
        /*! \brief     Default initializer.
        */
        explicit KernelService() : m_platform(0), m_ticks(0)
        { }

	#ifdef _STK_UNDER_TEST
        /*! \brief     Destructor.
            \note      It is used only when STK is under a test, should not be in production.
        */
        ~KernelService()
        {
        	// allow in multiple tests
        	SingletonBinder::Unbind(this);
        }
	#endif

        /*! \brief     Initialize instance.
            \note      When call completes Singleton<IKernelService *> will start referencing this
                       instance (see g_KernelService).
            \param[in] platform: Pointer to the IPlatform instance.
        */
        void Initialize(IPlatform *platform)
        {
            m_platform = platform;

            // make instance accessible for user
            SingletonBinder::Bind(this);
        }

        int64_t GetTicks() const { return m_ticks; }

        int32_t GetTicksResolution() const { return m_platform->GetTickResolution(); }

    private:
        void IncrementTick() { ++m_ticks; }

        IPlatform       *m_platform; //!< platform
        volatile int64_t m_ticks;    //!< CPU ticks elapsed (volatile is required for GCC O3 [at least] to avoid partial reads of the value by the consumer)
    };

public:
    /*! \brief Constants.
    */
    enum EConsts
    {
    	TASKS_MAX       = _Size,    //!< Maximum number of tasks supported by the instance of the Kernel.
        PERIODICITY_MAX = 60000000, //!< Maximum reasonable periodicity (microseconds), 60 seconds.
		PERIODICITY_DEFAULT = 1000, //!< Default reasonable periodicity (microseconds), 1 millisecond.
    };

    /*! \brief Default initializer.
    */
    explicit Kernel() : m_platform(NULL), m_switch_strategy(NULL), m_task_now(NULL)
    { }

    void Initialize(IPlatform *platform, ITaskSwitchStrategy *switch_strategy)
    {
        STK_ASSERT(platform != NULL);
        STK_ASSERT(switch_strategy != NULL);

        // allow initialization only once
        STK_ASSERT(!IsInitialized());

        m_platform        = platform;
        m_switch_strategy = switch_strategy;
        m_task_now        = NULL;
    }

    void AddTask(ITask *user_task)
    {
    	// must be initialized first
        STK_ASSERT(IsInitialized());

        KernelTask *task = AllocateNewTask(user_task);
        STK_ASSERT(task != NULL);

        m_switch_strategy->AddTask(task);
    }

    void Start(uint32_t resolution_us)
    {
        STK_ASSERT(resolution_us != 0);
        STK_ASSERT(resolution_us < PERIODICITY_MAX);
        STK_ASSERT(IsInitialized());

        m_task_now = static_cast<KernelTask *>(m_switch_strategy->GetFirst());
        STK_ASSERT(m_task_now != NULL);
        if (m_task_now == NULL)
            return;

        m_service.Initialize(m_platform);

        m_platform->Start(this, resolution_us, m_task_now);
    }

protected:
    KernelTask *AllocateNewTask(ITask *user_task)
    {
        STK_ASSERT(user_task != NULL);

        // look for a free kernel task
        KernelTask *kernel_task = NULL;
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->IsBusy())
            {
                // avoid task collision
                STK_ASSERT(task->m_user != user_task);
                if (task->m_user == user_task)
                    return NULL;

                // avoid stack collision
                STK_ASSERT(task->m_user->GetStack() != user_task->GetStack());
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
            STK_ASSERT(false);
            return NULL;
        }

        // init stack of the user task
        if (!m_platform->InitStack(&kernel_task->m_stack, user_task))
        {
            STK_ASSERT(false);
            return NULL;
        }

        // make kernel task busy with user task
        kernel_task->m_user = user_task;

        return kernel_task;
    }

    void UpdateAccessMode(KernelTask *now, KernelTask *next)
    {
        EAccessMode next_access_mode = next->GetUserTask()->GetAccessMode();

        if (now->GetUserTask()->GetAccessMode() != next_access_mode)
            m_platform->SetAccessMode(next_access_mode);
    }

    void OnStart()
    {
        m_platform->SetAccessMode(m_task_now->GetUserTask()->GetAccessMode());
    }

    void OnSysTick(Stack **idle, Stack **active)
    {
        KernelTask *now = m_task_now;
        KernelTask *next = static_cast<KernelTask *>(m_switch_strategy->GetNext(now));

        m_service.IncrementTick();

        if (next != now)
        {
            (*idle) = &now->m_stack;
            (*active) = &next->m_stack;

            m_task_now = next;

            UpdateAccessMode(now, next);

            m_platform->SwitchContext();
        }
    }

    bool IsInitialized() const
    {
    	return (m_platform != NULL) && (m_switch_strategy != NULL);
    }

    // If hit here: Kernel<N> expects at least 1 task, e.g. N > 0
	STK_STATIC_ASSERT(TASKS_MAX > 0);

    /*! \class TaskStorageType
        \brief KernelTask array type used as a storage for the KernelTask instances.
    */
    typedef KernelTask TaskStorageType[TASKS_MAX];

    KernelService        m_service;         //!< run-time kernel service
    IPlatform           *m_platform;        //!< platform driver
    ITaskSwitchStrategy *m_switch_strategy; //!< task switching strategy
    TaskStorageType      m_task_storage;    //!< task storage
    KernelTask          *m_task_now;        //!< current task task
};

} // namespace stk

#endif /* STK_H_ */
