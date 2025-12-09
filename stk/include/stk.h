/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_H_
#define STK_H_

#include "stk_helper.h"
#include "stk_arch.h"
#include "strategy/stk_strategy_rrobin.h"
#include "strategy/stk_strategy_swrrobin.h"

/*! \file  stk.h
    \brief Contains core implementation (Kernel) of the task scheduler.
*/

namespace stk {

/*! \class Kernel
    \brief Concrete implementation of IKernel (thread scheduling kernel).
    \note  Kernel expects at least 1 task, e.g. Kernel<N> where N != 0.

    Usage example:
    \code
    static Kernel<KERNEL_STATIC, 3> kernel;
    static PlatformArmCortexM platform;
    static SwitchStrategyRoundRobin tsstrategy;

    static Task<ACCESS_PRIVILEGED> task1;
    static Task<ACCESS_USER> task2;
    static Task<ACCESS_USER> task3;

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(PERIODICITY_DEFAULT);
    \endcode
*/
template <int32_t _Mode, uint32_t _Size, class _TyStrategy, class _TyPlatform>
class Kernel : public IKernel, private IPlatform::IEventHandler
{
    /*! \typedef TrapStackMemory
        \brief   Stack memory wrapper type of the Exit trap.
    */
    typedef StackMemoryWrapper<STACK_SIZE_MIN> TrapStackMemory;

    /*! \enum  ERequest
        \brief Request flags.
    */
    enum ERequest : uint8_t
    {
        REQUEST_NONE     = 0,       //!< none
        REQUEST_ADD_TASK = (1 << 0) //!< request for Kernel::AddTask is pending from some of the tasks
    };

    /*! \class KernelTask
        \brief Concrete implementation of the IKernelTask interface.
    */
    class KernelTask : public IKernelTask
    {
        friend class Kernel;

        /*! \enum  EStateFlags
            \brief Task state flags.
        */
        enum EStateFlags
        {
            STATE_NONE           = 0,       //!< none
            STATE_REMOVE_PENDING = (1 << 0) //!< task signaled that it exited
        };

    public:
        /*! \class AddTaskRequest
            \brief Serialized add task request.
            \note  Related to stk::KERNEL_STATIC or stk::KERNEL_DYNAMIC modes only.
        */
        struct AddTaskRequest
        {
            ITask *user_task; //!< user task to add
        };

        /*! \brief Default initializer.
        */
        explicit KernelTask() : m_user(NULL), m_stack(), m_state(STATE_NONE), m_time_sleep(0),
            m_srt(), m_hrt(), m_rt_weight() {}

        ITask *GetUserTask() { return m_user; }

        Stack *GetUserStack() { return &m_stack; }

        bool IsBusy() const { return (m_user != NULL); }

        void SetCurrentWeight(int32_t weight)
        {
            if (_TyStrategy::WEIGHT_API)
                m_rt_weight[0] = weight;
        }

        int32_t GetWeight() const { return (_TyStrategy::WEIGHT_API ? m_user->GetWeight() : 1); }

        int32_t GetCurrentWeight() const { return (_TyStrategy::WEIGHT_API ? m_rt_weight[0] : 1); }

    private:
        /*! \class SrtInfo
            \brief Soft Real-Time info of the bound task.
            \note  Related to non stk::KERNEL_HRT mode only.
        */
        struct SrtInfo
        {
            SrtInfo()
            {
                Clear();
            }

            /*! \brief     Clear values.
            */
            void Clear()
            {
                add_task_req = NULL;
            }

            AddTaskRequest *add_task_req; //!< add task request made from another active task (see Kernel::AddTask)
        };

        /*! \class HrtInfo
            \brief Hard Real-Time info of the bound task.
            \note  Related to stk::KERNEL_HRT mode only.
        */
        struct HrtInfo
        {
            HrtInfo()
            {
                Clear();
            }

            /*! \brief     Clear values.
            */
            void Clear()
            {
                periodicity = 0;
                deadline    = 0;
                duration    = 0;
                last_ticks  = 0;
            }

            int32_t periodicity; //!< scheduling periodicity (ticks)
            int32_t deadline;    //!< work deadline (ticks)
            int32_t duration;    //!< current duration of the active state when work is being carried out by the task (ticks)
            int64_t last_ticks;  //!< last saved tick value obtained by IKernelService::GetTicks (ticks)
        };

        /*! \brief     Release variables from info about previous task.
        */
        void Bind(_TyPlatform *platform, ITask *user_task)
        {
            // init stack of the user task
            if (!platform->InitStack(STACK_USER_TASK, &m_stack, user_task, user_task))
            {
                STK_ASSERT(false);
            }

            // bind user task
            m_user = user_task;

            // set access mode for this stack
            m_stack.mode = user_task->GetAccessMode();
        }

        /*! \brief     Release variables from info about previous task.
        */
        void Unbind()
        {
            m_user       = NULL;
            m_stack      = {};
            m_state      = STATE_NONE;
            m_time_sleep = 0;

            if (_Mode & KERNEL_HRT)
                m_hrt[0].Clear();
            else
                m_srt[0].Clear();
        }

        /*! \brief     Schedule the removal of the task from the kernel on next tick.
        */
        void ScheduleRemoval() { m_state |= STATE_REMOVE_PENDING; }

        /*! \brief     Check if task is pending removal.
        */
        bool IsPendingRemoval() const { return (m_state & STATE_REMOVE_PENDING) != 0; }

        /*! \brief     Check if Stack Pointer (SP) belongs to this task.
            \param[in] SP: Stack Pointer.
        */
        bool IsMemoryOfSP(size_t SP) const
        {
            size_t *start = m_user->GetStack();
            size_t *end   = start + m_user->GetStackSize();

            return (SP >= (size_t)start) && (SP <= (size_t)end);
        }

        /*! \brief     Initialize task with HRT info.
            \note      Related to stk::KERNEL_HRT mode only.
            \param[in] periodicity_tc: Periodicity time at which task is scheduled (ticks).
            \param[in] deadline_tc: Deadline time within which a task must complete its work (ticks).
            \param[in] start_delay_tc: Initial start delay for the task (ticks).
        */
        void HrtInit(uint32_t periodicity_tc, uint32_t deadline_tc, int32_t start_delay_tc)
        {
            m_hrt[0].periodicity = periodicity_tc;
            m_hrt[0].deadline    = deadline_tc;
            m_time_sleep         = -start_delay_tc;
        }

        /*! \brief     Called when task is switched into the scheduling process.
            \note      Related to stk::KERNEL_HRT mode only.
            \param[in] ticks: Current ticks of the Kernel.
        */
        void HrtOnSwitchedIn(int64_t ticks) { m_hrt[0].last_ticks = ticks; }

        /*! \brief     Called when task is switched out from the scheduling process.
            \note      Related to stk::KERNEL_HRT mode only.
            \param[in] platform: Platform driver instance.
            \param[in] ticks: Current ticks of the Kernel.
        */
        void HrtOnSwitchedOut(IPlatform *platform, int64_t ticks)
        {
            int32_t duration = m_hrt[0].duration + (int32_t)(ticks - m_hrt[0].last_ticks);
            m_hrt[0].duration = 0;

            STK_ASSERT(duration >= 0);

            // check if deadline is missed (HRT failure)
            if (HrtIsDeadlineMissed(duration))
            {
                m_user->OnDeadlineMissed(duration);
                platform->ProcessHardFault();
                STK_ASSERT(false);
            }

            m_time_sleep = -(m_hrt[0].periodicity - duration);
        }

        /*! \brief     Called when task process called IKernelService::SwitchToNext to inform Kernel that work is completed.
            \note      Related to stk::KERNEL_HRT mode only.
        */
        void HrtOnWorkCompleted() { m_time_sleep = -INT32_MAX; }

        /*! \brief     Check if deadline missed.
            \note      Related to stk::KERNEL_HRT mode only.
        */
        bool HrtIsDeadlineMissed(int32_t duration) const { return (duration > m_hrt[0].deadline); }

        ITask    *m_user;       //!< user task
        Stack     m_stack;      //!< stack descriptor
        uint32_t  m_state;      //!< state flags
        int32_t   m_time_sleep; //!< time to sleep (ticks)
        SrtInfo   m_srt[STK_ALLOCATE_COUNT(_Mode, KERNEL_HRT, 0, 1)]; //!< Soft Real-Time info (does not occupy memory if kernel operation mode is stk::KERNEL_HRT)
        HrtInfo   m_hrt[STK_ALLOCATE_COUNT(_Mode, KERNEL_HRT, 1, 0)]; //!< Hard Real-Time info (does not occupy memory if kernel operation mode is not stk::KERNEL_HRT)
        int32_t   m_rt_weight[_TyStrategy::WEIGHT_API ? 1 : 0];       //!< current (run-time) weight, see SwitchStrategySmoothWeightedRoundRobin
    };

    /*! \class KernelService
        \brief Concrete implementation of the IKernelService interface.
    */
    class KernelService : public IKernelService
    {
        friend class Kernel;

    public:
        size_t GetTid() const { return m_platform->GetCallerSP(); }

        int64_t GetTicks() const { return m_ticks; }

        int32_t GetTickResolution() const  { return m_platform->GetTickResolution(); }

        __stk_attr_noinline void Delay(uint32_t msec) const
        {
            int64_t deadline = GetTicks() + GetTicksFromMsec(msec, GetTickResolution());
            while (GetTicks() < deadline)
            {
                __stk_relax_cpu();
            }
        }

        __stk_attr_noinline void Sleep(uint32_t msec)
        {
            if ((_Mode & KERNEL_HRT) == 0)
            {
                m_platform->SleepTicks((uint32_t)GetTicksFromMsec(msec, GetTickResolution()));
            }
            else
            {
                // sleeping is not supported in HRT mode, task will sleep according to its periodicity and workload
                STK_ASSERT(false);
            }
        }

        void SwitchToNext() { m_platform->SwitchToNext(); }

    private:
        /*! \brief     Default initializer.
        */
        explicit KernelService() : m_platform(0), m_ticks(0) {}

        /*! \brief     Initialize instance.
            \note      When call completes Singleton<IKernelService *> will start referencing this
                       instance (see g_KernelService).
            \param[in] platform: IPlatform instance.
        */
        void Initialize(IPlatform *platform)
        {
            m_platform = static_cast<_TyPlatform *>(platform);
        }

        /*! \brief     Increment tick by 1.
        */
        void IncrementTick() { ++m_ticks; }

        _TyPlatform     *m_platform; //!< platform
        volatile int64_t m_ticks;    //!< CPU ticks elapsed (volatile to reload value from the memory by the consumer)
    };

public:
    /*! \brief Constants.
    */
    enum EConsts
    {
        TASKS_MAX = _Size //!< Maximum number of tasks supported by the instance of the Kernel.
    };

    /*! \brief Default initializer.
    */
    explicit Kernel() : m_platform(), m_strategy(), m_task_now(NULL), m_task_storage(), m_sleep_trap(),
        m_exit_trap(), m_fsm_state(FSM_STATE_NONE), m_request(~0)
    {
    #ifdef _DEBUG
        // _TyPlatform must inherit IPlatform
        IPlatform *platform = &m_platform;
        (void)platform;

        // _TyStrategy must inherit ITaskSwitchStrategy
        ITaskSwitchStrategy *strategy = &m_strategy;
        (void)strategy;
    #endif
    }

    __stk_attr_noinline void Initialize(uint32_t resolution_us = PERIODICITY_DEFAULT)
    {
        STK_ASSERT(resolution_us != 0);
        STK_ASSERT(resolution_us <= PERIODICITY_MAX);
        STK_ASSERT(!IsInitialized());

        m_task_now  = NULL;
        m_fsm_state = FSM_STATE_NONE;
        m_request   = REQUEST_NONE;

        m_service.Initialize(&m_platform);
        m_platform.Initialize(this, &m_service, resolution_us, (_Mode & KERNEL_DYNAMIC ? &m_exit_trap[0].stack : NULL));
    }

    __stk_attr_noinline void AddTask(ITask *user_task)
    {
        if ((_Mode & KERNEL_HRT) == 0)
        {
            STK_ASSERT(user_task != NULL);
            STK_ASSERT(IsInitialized());

            // when started the operation must be serialized by switching out from processing until
            // kernel processes this request
            if (IsStarted())
            {
                if ((_Mode & KERNEL_DYNAMIC) != 0)
                {
                    RequestAddTask(user_task);
                }
                else
                {
                    STK_ASSERT(false);
                }
            }
            else
            {
                AllocateAndAddNewTask(user_task);
            }
        }
        else
        {
            STK_ASSERT(false);
        }
    }

    __stk_attr_noinline void AddTask(ITask *user_task, int32_t periodicity_tc, int32_t deadline_tc, int32_t start_delay_tc)
    {
        if (_Mode & KERNEL_HRT)
        {
            STK_ASSERT(periodicity_tc > 0);
            STK_ASSERT(deadline_tc > 0);
            STK_ASSERT(start_delay_tc >= 0);
            STK_ASSERT(periodicity_tc < INT32_MAX);
            STK_ASSERT(deadline_tc < INT32_MAX);
            STK_ASSERT(user_task != NULL);
            STK_ASSERT(IsInitialized());
            STK_ASSERT(!IsStarted());

            AllocateAndAddNewTask(user_task)->HrtInit(periodicity_tc, deadline_tc, start_delay_tc);
        }
        else
        {
            STK_ASSERT(false);
        }
    }

    __stk_attr_noinline void RemoveTask(ITask *user_task)
    {
        if (_Mode & KERNEL_DYNAMIC)
        {
            STK_ASSERT(user_task != NULL);
            STK_ASSERT(!IsStarted());

            KernelTask *task = FindTask(user_task);
            if (task != NULL)
                RemoveTask(task);
        }
        else
        {
            // kernel operating mode must be KERNEL_DYNAMIC for tasks to be able to be removed
            STK_ASSERT(false);
        }
    }

    __stk_attr_noinline void Start()
    {
        STK_ASSERT(IsInitialized());

        m_task_now = NULL;

         // stacks of the traps must be re-initilized on every subsequent Start
        InitTraps();

        m_platform.Start();
    }

    bool IsStarted() const { return (m_task_now != NULL); }

    IPlatform *GetPlatform() { return &m_platform; }

protected:
    /*! \enum  EFsmState
        \brief Finite-state machine (FSM) state.
    */
    enum EFsmState : int8_t
    {
        FSM_STATE_NONE = -1,
        FSM_STATE_SWITCHING,
        FSM_STATE_SLEEPING,
        FSM_STATE_WAKING,
        FSM_STATE_EXITING,
        FSM_STATE_MAX
    };

    /*! \enum  EFsmEvent
        \brief Finite-state machine (FSM) event.
    */
    enum EFsmEvent : int8_t
    {
        FSM_EVENT_SWITCH = 0,
        FSM_EVENT_SLEEP,
        FSM_EVENT_WAKE,
        FSM_EVENT_EXIT,
        FSM_EVENT_MAX
    };

    /*! \brief     Initialize stack of the traps.
    */
    __stk_attr_noinline void InitTraps()
    {
        // init stack for a Sleep trap
        {
            TrapStack &sleep = m_sleep_trap[0];

            TrapStackMemory wrapper(&sleep.memory);
            sleep.stack.mode = ACCESS_PRIVILEGED;

            m_platform.InitStack(STACK_SLEEP_TRAP, &sleep.stack, &wrapper, NULL);
        }

        // init stack for an Exit trap
        if (_Mode & KERNEL_DYNAMIC)
        {
            TrapStack &exit = m_exit_trap[0];

            TrapStackMemory wrapper(&exit.memory);
            exit.stack.mode = ACCESS_PRIVILEGED;

            m_platform.InitStack(STACK_EXIT_TRAP, &exit.stack, &wrapper, NULL);
        }
    }

    /*! \brief     Allocate new instance of KernelTask.
        \param[in] user_task: User task for which kernel task object is allocated.
        \return    Kernel task.
    */
    KernelTask *AllocateNewTask(ITask *user_task)
    {
        // look for a free kernel task
        KernelTask *new_task = NULL;
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->IsBusy())
            {
                // avoid task collision
                STK_ASSERT(task->m_user != user_task);

                // avoid stack collision
                STK_ASSERT(task->m_user->GetStack() != user_task->GetStack());
            }
            else
            if (new_task == NULL)
            {
                new_task = task;
            #if defined(NDEBUG) && !defined(_STK_ASSERT_REDIRECT)
                break; // break if assertions are inactive and do not try to validate collision with existing tasks
            #endif
            }
        }

        // if NULL - exceeded max supported kernel task count, application design failure
        STK_ASSERT(new_task != NULL);

        new_task->Bind(&m_platform, user_task);

        return new_task;
    }

    /*! \brief     Allocate new instance of KernelTask and add it into the scheduling process.
        \param[in] user_task: User task for which kernel task object is allocated.
        \return    Kernel task.
    */
    KernelTask *AllocateAndAddNewTask(ITask *user_task)
    {
        KernelTask *task = AllocateNewTask(user_task);
        STK_ASSERT(task != NULL);

        m_strategy.AddTask(task);
        return task;
    }

    /*! \brief     Request to add new task.
        \note      Must be called by the task process only!
        \param[in] user_task: User task to add.
    */
    __stk_attr_noinline void RequestAddTask(ITask *user_task)
    {
        KernelTask *caller = FindTaskBySP(m_platform.GetCallerSP());
        STK_ASSERT(caller != NULL);

        typename KernelTask::AddTaskRequest req = { .user_task = user_task };
        caller->m_srt[0].add_task_req = &req;

        // notify kernel
        ScheduleAddTask();

        // switch out and wait for completion (due to context switch request could be processed here)
        __stk_full_memfence();
        if (caller->m_srt[0].add_task_req != NULL)
            m_platform.SwitchToNext();

        STK_ASSERT(caller->m_srt[0].add_task_req == NULL);
    }

    /*! \brief     Find kernel task for the bound ITask instance.
        \param[in] user_task: User task.
        \return    Kernel task.
    */
    __stk_attr_noinline KernelTask *FindTask(const ITask *user_task)
    {
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->GetUserTask() == user_task)
                return task;
        }

        return NULL;
    }

    /*! \brief     Find kernel task for the bound Stack instance.
        \param[in] stack: Stack.
        \return    Kernel task.
    */
    KernelTask *FindTask(const Stack *stack)
    {
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->GetUserStack() == stack)
                return task;
        }

        return NULL;
    }

    /*! \brief     Find kernel task for a Stack Pointer (SP).
        \param[in] SP: Stack pointer.
        \return    Kernel task.
    */
    __stk_attr_noinline KernelTask *FindTaskBySP(size_t SP)
    {
        if (m_task_now->IsMemoryOfSP(SP))
            return m_task_now;

        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];

            // skip finished tasks (applicable only for KERNEL_DYNAMIC mode)
            if ((_Mode & KERNEL_DYNAMIC) && !task->IsBusy())
                continue;

            if (task->IsMemoryOfSP(SP))
                return task;
        }

        return NULL;
    }

    /*! \brief     Remove kernel task.
        \note      Removal of the kernel task means releasing it from the user task details.
        \param[in] task: Kernel task.
    */
    void RemoveTask(KernelTask *task)
    {
        STK_ASSERT(task != NULL);

        m_strategy.RemoveTask(task);
        task->Unbind();
    }

    __stk_attr_noinline void OnStart(Stack **active)
    {
        STK_ASSERT(m_strategy.GetSize() != 0);

        m_task_now = static_cast<KernelTask *>(m_strategy.GetFirst());
        STK_ASSERT(m_task_now != NULL);

        // init FSM start state
        m_fsm_state = FSM_STATE_SWITCHING;

        // in HRT mode all tasks can have delayed start therefore get correct one
        if (_Mode & KERNEL_HRT)
        {
            KernelTask *next = NULL;
            m_fsm_state = GetNewFsmState(&next);

            // expecting only SLEEPING or SWITCHING states
            STK_ASSERT((m_fsm_state == FSM_STATE_SLEEPING) || (m_fsm_state == FSM_STATE_SWITCHING));
        }

        if (m_fsm_state == FSM_STATE_SWITCHING)
        {
            (*active) = m_task_now->GetUserStack();

            if (_Mode & KERNEL_HRT)
            {
                m_task_now->HrtOnSwitchedIn(m_service.GetTicks());
            }
        }
        else
        if (m_fsm_state == FSM_STATE_SLEEPING)
        {
            (*active) = &m_sleep_trap[0].stack;
        }
    }

    bool OnTick(Stack **idle, Stack **active)
    {
        m_service.IncrementTick();
        UpdateTasks();
        return UpdateFsmState(idle, active);
    }

    void OnTaskSwitch(size_t caller_SP)
    {
        OnTaskSleep(caller_SP, 1);
    }

    void OnTaskSleep(size_t caller_SP, int32_t ticks)
    {
        KernelTask *task = FindTaskBySP(caller_SP);
        STK_ASSERT(task != NULL);

        if (_Mode & KERNEL_HRT)
        {
            task->HrtOnWorkCompleted();
        }

        task->m_time_sleep -= ticks;

        while (task->m_time_sleep < 0)
        {
            __stk_relax_cpu();
        }
    }

    void OnTaskExit(Stack *stack)
    {
        if (_Mode & KERNEL_DYNAMIC)
        {
            KernelTask *task = FindTask(stack);
            STK_ASSERT(task != NULL);

            task->ScheduleRemoval();
        }
        else
        {
            // kernel operating mode must be KERNEL_DYNAMIC for tasks to be able to exit
            STK_ASSERT(false);
        }
    }

    /*! \brief     Update tasks (sleep, requests).
    */
    void UpdateTasks()
    {
        UpdateTaskRequest();
        UpdateTaskSleep();
    }

    /*! \brief     Update sleep timers of the sleeping tasks.
    */
    void UpdateTaskSleep()
    {
        for (int32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];

            // advance by +1 millisecond
            if (task->m_time_sleep < 0)
                ++task->m_time_sleep;
        }
    }

    /*! \brief     Update pending task requests.
    */
    void UpdateTaskRequest()
    {
        if (m_request == REQUEST_NONE)
            return;

        for (int32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];

            // process serialized AddTask request made from another active task, requesting process
            // is currently waiting due to SwitchToNext()
            if (((_Mode & KERNEL_HRT) == 0) && ((_Mode & KERNEL_DYNAMIC) != 0))
            {
                if (task->m_srt[0].add_task_req != NULL)
                {
                    AllocateAndAddNewTask(task->m_srt[0].add_task_req->user_task);
                    task->m_srt[0].add_task_req = NULL;
                }
            }
        }

        m_request = REQUEST_NONE;
    }

    /*! \brief     Fetch next event for the FSM.
        \param[in] next: Next kernel task to which Kernel can switch.
        \return    FSM event.
    */
    EFsmEvent FetchNextEvent(KernelTask **next)
    {
        EFsmEvent type = FSM_EVENT_SWITCH;
        KernelTask *itr = m_task_now, *prev = m_task_now, *sleep_end = NULL, *pending_end = NULL;

        for (;;)
        {
            if (_Mode & KERNEL_DYNAMIC)
            {
                for (;;)
                {
                    itr = static_cast<KernelTask *>(m_strategy.GetNext(prev));

                    // process pending task removal
                    if (itr->IsPendingRemoval())
                    {
                        // we can't remove current task because task switching driver context is branchless
                        // therefore make any other task as current, switch to it and then remove pending
                        if ((itr == m_task_now) && (itr != pending_end))
                        {
                            // memorize as end to avoid endless loop if all entries are pending exit
                            if (pending_end == NULL)
                                pending_end = itr;

                            if (_Mode & KERNEL_HRT)
                            {
                                // current task will not be switched out in StateSwitch because it is the last one
                                // therefore make sure deadline is checked for this task
                                if (m_strategy.GetSize() == 1)
                                    itr->HrtOnSwitchedOut(&m_platform, m_service.GetTicks());
                            }

                            // move to the next
                            prev = itr;
                            continue;
                        }

                        RemoveTask(itr);

                        // reset pending end to restore looping to the next non-pending
                        pending_end = NULL;

                        // check if no tasks left
                        if (m_strategy.GetSize() == 0)
                        {
                            itr  = NULL;
                            type = FSM_EVENT_EXIT;
                            break;
                        }

                        continue;
                    }

                    break;
                }
            }
            else
            {
                itr = static_cast<KernelTask *>(m_strategy.GetNext(prev));
            }

            // check if task is sleeping
            if ((itr != NULL) && (itr->m_time_sleep < 0))
            {
                // if iterated back to self then all tasks are sleeping and kernel should enter a sleep mode
                if (itr == sleep_end)
                {
                    itr  = NULL;
                    type = FSM_EVENT_SLEEP;
                    break;
                }

                // memorize as end to avoid endless loop if all entries are sleeping
                if (sleep_end == NULL)
                    sleep_end = itr;

                prev = itr;
                continue;
            }

            // if was sleeping send wake event first
            if (m_fsm_state == FSM_STATE_SLEEPING)
                type = FSM_EVENT_WAKE;

            break;
        }

        (*next) = itr;
        return type;
    }

    /*! \brief     Get new FSM state.
        \param[in] next: Next kernel task to which Kernel can switch.
        \return    FSM state.
    */
    EFsmState GetNewFsmState(KernelTask **next)
    {
        STK_ASSERT(m_fsm_state != FSM_STATE_NONE);
        return m_fsm[m_fsm_state][FetchNextEvent(next)];
    }

    /*! \brief      Update FSM state.
        \param[out] idle: Stack of the task which must enter Idle state.
        \param[out] active: Stack of the task which must enter Active state (to which context will switch).
        \return     FSM state.
    */
    bool UpdateFsmState(Stack **idle, Stack **active)
    {
        KernelTask *now = m_task_now, *next;
        EFsmState new_state = GetNewFsmState(&next);
        bool switch_context;

        switch (new_state)
        {
        case FSM_STATE_SWITCHING: {
            switch_context = StateSwitch(now, next, idle, active);
            break; }

        case FSM_STATE_WAKING: {
            switch_context = StateWake(now, next, idle, active);
            break; }

        case FSM_STATE_SLEEPING: {
            switch_context = StateSleep(now, next, idle, active);
            break; }

        case FSM_STATE_EXITING: {
            switch_context = StateExit(now, next, idle, active);
            break; }

        default:
            return false;
        }

        m_fsm_state = new_state;
        return switch_context;
    }

    /*! \brief      Switches contexts.
        \note       FSM state: stk::FSM_STATE_SWITCHING.
        \param[in]  now: Currently active kernel task.
        \param[in]  next: Next kernel task.
        \param[out] idle: Stack of the task which must enter Idle state.
        \param[out] active: Stack of the task which must enter Active state (to which context will switch).
    */
    bool StateSwitch(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        STK_ASSERT(now != NULL);
        STK_ASSERT(next != NULL);

        // do not switch context because task did not change
        if (next == now)
            return false;

        (*idle)   = now->GetUserStack();
        (*active) = next->GetUserStack();

        // if stack memory is exceeded these assertions will be hit
        STK_ASSERT(now->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);
        STK_ASSERT(next->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);

        m_task_now = next;

        if (_Mode & KERNEL_HRT)
        {
            int64_t ticks = m_service.GetTicks();

            now->HrtOnSwitchedOut(&m_platform, ticks);
            next->HrtOnSwitchedIn(ticks);
        }

        return true; // switch context
    }

    /*! \brief      Wakes up after sleeping.
        \note       FSM state: stk::FSM_STATE_AWAKENING.
        \param[in]  now: Currently active kernel task (ignored).
        \param[in]  next: Next kernel task.
        \param[out] idle: Stack of the task which must enter Idle state.
        \param[out] active: Stack of the task which must enter Active state (to which context will switch).
    */
    bool StateWake(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)now;

        STK_ASSERT(next != NULL);

        (*idle)   = &m_sleep_trap[0].stack;
        (*active) = next->GetUserStack();

        // if stack memory is exceeded these assertions will be hit
        STK_ASSERT(m_sleep_trap[0].memory[0] == STK_STACK_MEMORY_FILLER);
        STK_ASSERT(next->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);

        m_task_now = next;

        if (_Mode & KERNEL_HRT)
        {
            next->HrtOnSwitchedIn(m_service.GetTicks());
        }

        return true; // switch context
    }

    /*! \brief      Enters into a sleeping mode.
        \note       FSM state: stk::FSM_STATE_SLEEPING.
        \param[in]  now: Currently active kernel task.
        \param[in]  next: Next kernel task (ignored).
        \param[out] idle: Stack of the task which must enter Idle state.
        \param[out] active: Stack of the task which must enter Active state (to which context will switch).
    */
    bool StateSleep(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)next;

        STK_ASSERT(now != NULL);
        STK_ASSERT(m_sleep_trap[0].stack.SP != 0);

        (*idle)   = now->GetUserStack();
        (*active) = &m_sleep_trap[0].stack;

        m_task_now = static_cast<KernelTask *>(m_strategy.GetFirst());

        if (_Mode & KERNEL_HRT)
        {
            now->HrtOnSwitchedOut(&m_platform, m_service.GetTicks());
        }

        return true; // switch context
    }

    /*! \brief      Exits from scheduling.
        \note       FSM state: stk::FSM_STATE_EXITING.
        \note       Exits only if stk::KERNEL_DYNAMIC mode is specified, otherwise ignored.
        \param[in]  now: Currently active kernel task (ignored).
        \param[in]  next: Next kernel task (ignored).
        \param[out] idle: Stack of the task which must enter Idle state.
        \param[out] active: Stack of the task which must enter Active state (to which context will switch).
    */
    bool StateExit(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)now;
        (void)next;

        if (_Mode & KERNEL_DYNAMIC)
        {
            // dynamic tasks are not supported if main processes's stack memory is not provided in Start()
            STK_ASSERT(m_exit_trap[0].stack.SP != 0);

            (*idle)   = NULL;
            (*active) = &m_exit_trap[0].stack;

            m_task_now = NULL;

            m_platform.Stop();
        }
        else
        {
            (void)idle;
            (void)active;
        }

        return false;
    }

#ifdef _STK_UNDER_TEST
    /*! \brief     Get switch strategy instance.
        \return    Pointer to the ITaskSwitchStrategy concrete class instance.
    */
    ITaskSwitchStrategy *GetSwitchStrategy() { return &m_strategy; }
#endif

    /*! \brief     Check if kernel was initialized with IKernel::Initialize().
        \return    True if initialized, otherwise false.
    */
    bool IsInitialized() const { return (m_request == REQUEST_NONE); }

    /*! \brief     Schedule processing of the add task request.
    */
    void ScheduleAddTask() { m_request |= REQUEST_ADD_TASK; }

    // If hit here: Kernel<N> expects at least 1 task, e.g. N > 0
    STK_STATIC_ASSERT_N(TASKS_MAX, TASKS_MAX > 0);

    // If hit here: Kernel mode must be assigned.
    STK_STATIC_ASSERT_N(KENREL_MODE_MUST_BE_SET, (_Mode != 0));

    // If hit here: KERNEL_STATIC and KERNEL_DYNAMIC can not be mixed, either one of these is possible.
    STK_STATIC_ASSERT_N(KENREL_MODE_MIX_NOT_ALLOWED,
        (((_Mode & KERNEL_STATIC) & (_Mode & KERNEL_DYNAMIC)) == 0));

    // If hit here: KERNEL_HRT must accompany KERNEL_STATIC or KERNEL_DYNAMIC.
    STK_STATIC_ASSERT_N(KENREL_MODE_HRT_ALONE, ((_Mode & KERNEL_HRT) == 0) ||
        ((_Mode & KERNEL_HRT) && ((_Mode & KERNEL_STATIC) || (_Mode & KERNEL_DYNAMIC))));

    /*! \typedef TaskStorageType
        \brief   KernelTask array type used as a storage for the KernelTask instances.
    */
    typedef KernelTask TaskStorageType[TASKS_MAX];

    /*! \class TrapStack
        \brief Trap stack is used to execute exit, sleep traps when required.

        \note  Exit trap - used for an exit into the main process from which IKernel::Start() was called
               when all tasks completed their processing and exited by returning from their Run function.

               Sleep trap - used to execute a sleep procedure of the driver when all user tasks are currently
               in a sleep state.
    */
    struct TrapStack
    {
        typedef TrapStackMemory::MemoryType Memory;

        Stack  stack;  //!< stack information
        Memory memory; //!< stack memory
    };

    KernelService   m_service;         //!< run-time kernel service
    _TyPlatform     m_platform;        //!< platform driver
    _TyStrategy     m_strategy;        //!< task switching strategy
    KernelTask     *m_task_now;        //!< current task task
    TaskStorageType m_task_storage;    //!< task storage
    TrapStack       m_sleep_trap[1];   //!< sleep trap
    TrapStack       m_exit_trap[STK_ALLOCATE_COUNT(_Mode, KERNEL_DYNAMIC, 1, 0)]; //!< exit trap (does not occupy memory if kernel operation mode is not KERNEL_DYNAMIC)
    EFsmState       m_fsm_state;       //!< FSM state
    uint32_t        m_request;         //!< pending requests from the tasks

    const EFsmState m_fsm[FSM_STATE_MAX][FSM_EVENT_MAX] = {
    //    FSM_EVENT_SWITCH     FSM_EVENT_SLEEP     FSM_EVENT_WAKE    FSM_EVENT_EXIT
        { FSM_STATE_SWITCHING, FSM_STATE_SLEEPING, FSM_STATE_NONE,   FSM_STATE_EXITING }, // FSM_STATE_SWITCHING
        { FSM_STATE_NONE,      FSM_STATE_NONE,     FSM_STATE_WAKING, FSM_STATE_NONE },    // FSM_STATE_SLEEPING
        { FSM_STATE_SWITCHING, FSM_STATE_SLEEPING, FSM_STATE_NONE,   FSM_STATE_EXITING }, // FSM_STATE_WAKING
        { FSM_STATE_NONE,      FSM_STATE_NONE,     FSM_STATE_NONE,   FSM_STATE_NONE }     // FSM_STATE_EXITING
    }; //!< FSM state table (Kernel implements table-based FSM)
};

} // namespace stk

#endif /* STK_H_ */
