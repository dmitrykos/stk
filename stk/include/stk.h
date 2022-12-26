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
#include "strategy/stk_strategy_rrobin.h"

/*! \file  stk.h
    \brief Contains core implementation (Kernel) of the task scheduler.
*/

namespace stk {

/*! \def   g_KernelService
    \brief Pointer to the IKernelService instance which becomes available when scheduling
           is started with Kernel::Start().
    \note  Singleton design pattern.
*/
#define g_KernelService stk::Singleton<stk::IKernelService *>::Get()

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
template <EKernelMode _Mode, uint32_t _Size>
class Kernel : public IKernel, private IPlatform::IEventHandler
{
    /*! \typedef TrapStackStackMemory
        \brief   Stack memory wrapper type of the Exit trap.
    */
    typedef StackMemoryWrapper<STACK_SIZE_MIN> TrapStackStackMemory;

    /*! \class KernelTask
        \brief Concrete implementation of the IKernelTask interface.
    */
    class KernelTask : public IKernelTask
    {
        friend class Kernel;

        enum EStateFlags
        {
            STATE_NONE           = 0,
            STATE_REMOVE_PENDING = (1 << 0)
        };

    public:
        /*! \brief Default initializer.
        */
        explicit KernelTask() : m_user(NULL), m_state(STATE_NONE), m_stack(), m_time_sleep(0)
        { }

        ITask *GetUserTask() { return m_user; }

        Stack *GetUserStack() { return &m_stack; }

        bool IsBusy() const { return (m_user != NULL); }

    private:
        void Unbind()
        {
            Clear();
        }

        void Clear()
        {
            m_user       = NULL;
            m_stack      = {};
            m_state      = STATE_NONE;
            m_time_sleep = 0;
        }

        void ScheduleRemoval() { m_state |= STATE_REMOVE_PENDING; }

        bool IsPendingRemoval() const { return (m_state & STATE_REMOVE_PENDING) != 0; }

        bool IsMemoryOfSP(size_t caller_SP) const
        {
            size_t *start = m_user->GetStack();
            size_t *end   = start + m_user->GetStackSize();

            return (caller_SP >= (size_t)start) && (caller_SP <= (size_t)end);
        }

        ITask   *m_user;       //!< user task
        uint32_t m_state;      //!< state flags
        Stack    m_stack;      //!< stack descriptor
        int32_t  m_time_sleep; //!< time to sleep
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
            template <EKernelMode _Mode0, uint32_t _Size0> friend class Kernel;
        };

    public:
        int64_t GetTicks() const
        {
            return m_ticks;
        }

        int32_t GetTickResolution() const
        {
            return m_platform->GetTickResolution();
        }

        void Sleep(uint32_t sleep_ms)
        {
            m_platform->SleepTicks((uint32_t)ConvertMicrosecondsToTicks(sleep_ms * 1000));
        }

        void SwitchToNext()
        {
            m_platform->SwitchToNext();
        }

    private:
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

            // make instance accessible for the user
            if (Singleton<IKernelService *>::Get() == NULL)
                SingletonBinder::Bind(this);
        }

        void IncrementTick()
        {
            ++m_ticks;
        }

        IPlatform       *m_platform; //!< platform
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
    explicit Kernel() : m_platform(NULL), m_switch_strategy(NULL), m_task_now(NULL), m_sleep_trap(), m_exit_trap(),
        m_fsm_state(FSM_STATE_NONE)
    { }

    void Initialize(IPlatform *platform, ITaskSwitchStrategy *switch_strategy)
    {
        STK_ASSERT(platform != NULL);
        STK_ASSERT(switch_strategy != NULL);
        STK_ASSERT(!IsInitialized());

        m_platform        = platform;
        m_switch_strategy = switch_strategy;
        m_task_now        = NULL;
        m_fsm_state       = FSM_STATE_NONE;
    }

    void AddTask(ITask *user_task)
    {
        STK_ASSERT(user_task != NULL);
        STK_ASSERT(IsInitialized());

        KernelTask *task = AllocateNewTask(user_task);
        STK_ASSERT(task != NULL);

        m_switch_strategy->AddTask(task);
    }

    void RemoveTask(ITask *user_task)
    {
        if (_Mode == KERNEL_DYNAMIC)
        {
            STK_ASSERT(user_task != NULL);

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

    void Start(uint32_t resolution_us = PERIODICITY_DEFAULT)
    {
        STK_ASSERT(resolution_us != 0);
        STK_ASSERT(resolution_us <= PERIODICITY_MAX);
        STK_ASSERT(IsInitialized());

        m_task_now = static_cast<KernelTask *>(m_switch_strategy->GetFirst());
        STK_ASSERT(m_task_now != NULL);

        // stacks of the traps must be re-initilized on every subsequent Start
        InitTraps();

        // make sure SFM is in switching state after re-start
        m_fsm_state = FSM_STATE_SWITCHING;

        m_service.Initialize(m_platform);

        m_platform->Start(this, resolution_us, m_task_now, (_Mode == KERNEL_DYNAMIC ? &m_exit_trap[0].stack : NULL));
    }

protected:
    enum EFsmState : int8_t
    {
        FSM_STATE_NONE = -1,
        FSM_STATE_SWITCHING,
        FSM_STATE_SLEEPING,
        FSM_STATE_AWAKENING,
        FSM_STATE_EXITING,
        FSM_STATE_MAX
    };

    enum EFsmEvent : int8_t
    {
        FSM_EVENT_SWITCH = 0,
        FSM_EVENT_SLEEP,
        FSM_EVENT_WAKE,
        FSM_EVENT_EXIT,
        FSM_EVENT_MAX
    };

    void InitTraps()
    {
        // init stack for a Sleep trap
        {
            TrapStackStackMemory wrapper(&m_sleep_trap[0].memory);
            m_platform->InitStack(STACK_SLEEP_TRAP, &m_sleep_trap[0].stack, &wrapper, NULL);
        }

        // init stack for an Exit trap
        if (_Mode == KERNEL_DYNAMIC)
        {
            TrapStackStackMemory wrapper(&m_exit_trap[0].memory);
            m_platform->InitStack(STACK_EXIT_TRAP, &m_exit_trap[0].stack, &wrapper, NULL);
        }
    }

    KernelTask *AllocateNewTask(ITask *user_task)
    {
        // look for a free kernel task
        KernelTask *kernel_task = NULL;
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
            if (kernel_task == NULL)
            {
                kernel_task = task;
            }
        }

        // if NULL - exceeded max supported kernel task count, application design failure
        STK_ASSERT(kernel_task != NULL);

        // init stack of the user task
        if (!m_platform->InitStack(STACK_USER_TASK, kernel_task->GetUserStack(), user_task, user_task))
        {
            STK_ASSERT(false);
        }

        // make kernel task busy with user task
        kernel_task->m_user = user_task;

        return kernel_task;
    }

    KernelTask *FindTask(const ITask *user_task)
    {
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->GetUserTask() == user_task)
                return task;
        }

        return NULL;
    }

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

    KernelTask *FindTaskBySP(size_t SP)
    {
        for (uint32_t i = 0; i < TASKS_MAX; ++i)
        {
            KernelTask *task = &m_task_storage[i];
            if (task->IsMemoryOfSP(SP))
                return task;
        }

        return NULL;
    }

    KernelTask *GetTaskForSP(size_t caller_SP)
    {
        if (m_task_now->IsMemoryOfSP(caller_SP))
            return m_task_now;

        return FindTaskBySP(caller_SP);
    }

    void RemoveTask(KernelTask *task)
    {
        STK_ASSERT(task != NULL);

        m_switch_strategy->RemoveTask(task);
        task->Unbind();
    }

    void UpdateAccessMode(KernelTask *task)
    {
        m_platform->SetAccessMode(task->GetUserTask()->GetAccessMode());
    }

    void OnStart()
    {
        UpdateAccessMode(m_task_now);
    }

    void OnSysTick(Stack **idle, Stack **active)
    {
        m_service.IncrementTick();
        SwitchTask(idle, active, 1);
    }

    void OnTaskSwitch(size_t caller_SP)
    {
        OnTaskSleep(caller_SP, 1);
    }

    void OnTaskSleep(size_t caller_SP, uint32_t sleep_ticks)
    {
        KernelTask *task = GetTaskForSP(caller_SP);
        STK_ASSERT(task != NULL);

        task->m_time_sleep -= sleep_ticks;

        while (task->m_time_sleep < 0)
        {
            __stk_relax_cpu();
        }
    }

    void OnTaskExit(Stack *stack)
    {
        if (_Mode == KERNEL_DYNAMIC)
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

    EFsmEvent FetchNextEvent(int32_t time_tick, KernelTask **next)
    {
        EFsmEvent type = FSM_EVENT_SWITCH;
        KernelTask *itr = m_task_now, *prev = m_task_now, *end = NULL;

        for (;;)
        {
            if (_Mode == KERNEL_DYNAMIC)
            {
                while ((itr = static_cast<KernelTask *>(m_switch_strategy->GetNext(prev))) != NULL)
                {
                    // process pending task removal
                    if (itr->IsPendingRemoval())
                    {
                        RemoveTask(itr);

                        // check if current task and clear its pointer
                        if (itr == m_task_now)
                            m_task_now = NULL;

                        // check if no tasks left
                        if (m_switch_strategy->GetFirst() == NULL)
                        {
                            itr        = NULL;
                            m_task_now = NULL;
                            type       = FSM_EVENT_EXIT;
                            break;
                        }

                        continue;
                    }
                    else
                        break;
                }
            }
            else
            {
                itr = static_cast<KernelTask *>(m_switch_strategy->GetNext(prev));
            }

            // check if task is sleeping
            if ((itr != NULL) && (itr->m_time_sleep < 0))
            {
                if (itr == end)
                {
                    itr  = NULL;
                    type = FSM_EVENT_SLEEP;
                    break;
                }

                if (end == NULL)
                    end = itr;

                itr->m_time_sleep += time_tick;

                // if still need to sleep get next
                if (itr->m_time_sleep < 0)
                {
                    prev = itr;
                    continue;
                }

                itr->m_time_sleep = 0;

                // if was sleeping send wake event first
                if (m_fsm_state == FSM_STATE_SLEEPING)
                    type = FSM_EVENT_WAKE;
            }

            break;
        }

        (*next) = itr;
        return type;
    }

    void SwitchTask(Stack **idle, Stack **active, int32_t time_tick)
    {
        KernelTask *now = m_task_now, *next;

        EFsmState new_state = m_fsm[m_fsm_state][FetchNextEvent(time_tick, &next)];
        if (new_state != FSM_STATE_NONE)
            m_fsm_state = new_state;

        switch (new_state)
        {
        case FSM_STATE_SWITCHING: {
            if (next != now)
                StateSwitch(now, next, idle, active);
            break; }

        case FSM_STATE_AWAKENING: {
            StateAwaken(now, next, idle, active);
            break; }

        case FSM_STATE_SLEEPING: {
            StateSleep(now, next, idle, active);
            break; }

        case FSM_STATE_EXITING: {
            StateExit(now, next, idle, active);
            break; }

        default:
            break;
        }
    }

    void StateSwitch(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (*idle)   = now->GetUserStack();
        (*active) = next->GetUserStack();

        // if stack memory is exceeded these assertions will be hit
        STK_ASSERT(now->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);
        STK_ASSERT(next->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);

        m_task_now = next;

        UpdateAccessMode(next);
        m_platform->SwitchContext();
    }

    void StateAwaken(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)now;

        (*idle)   = &m_sleep_trap[0].stack;
        (*active) = next->GetUserStack();

        // if stack memory is exceeded these assertions will be hit
        STK_ASSERT(m_sleep_trap[0].memory[0] == STK_STACK_MEMORY_FILLER);
        STK_ASSERT(next->GetUserTask()->GetStack()[0] == STK_STACK_MEMORY_FILLER);

        m_task_now = next;

        UpdateAccessMode(next);
        m_platform->SwitchContext();
    }

    void StateSleep(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)now;
        (void)next;

        STK_ASSERT(m_sleep_trap[0].stack.SP != 0);

        (*idle)   = now->GetUserStack();
        (*active) = &m_sleep_trap[0].stack;

        m_task_now = static_cast<KernelTask *>(m_switch_strategy->GetFirst());

        m_platform->SetAccessMode(ACCESS_PRIVILEGED);
        m_platform->SwitchContext();
    }

    void StateExit(KernelTask *now, KernelTask *next, Stack **idle, Stack **active)
    {
        (void)now;
        (void)next;

        if (_Mode == KERNEL_DYNAMIC)
        {
            // dynamic tasks are not supported if main processes's stack memory is not provided in Start()
            STK_ASSERT(m_exit_trap[0].stack.SP != 0);

            (*idle)   = NULL;
            (*active) = &m_exit_trap[0].stack;

            m_platform->SetAccessMode(ACCESS_PRIVILEGED);
            m_platform->Stop();
        }
    }

    bool IsInitialized() const
    {
        return (m_platform != NULL) && (m_switch_strategy != NULL);
    }

    // If hit here: Kernel<N> expects at least 1 task, e.g. N > 0
    STK_STATIC_ASSERT(TASKS_MAX > 0);

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
        typedef TrapStackStackMemory::MemoryType Memory;

        Stack  stack;  //!< stack information
        Memory memory; //!< stack memory
    };

    KernelService        m_service;         //!< run-time kernel service
    IPlatform           *m_platform;        //!< platform driver
    ITaskSwitchStrategy *m_switch_strategy; //!< task switching strategy
    KernelTask          *m_task_now;        //!< current task task
    TaskStorageType      m_task_storage;    //!< task storage
    TrapStack            m_sleep_trap[1];   //!< sleep trap
    TrapStack            m_exit_trap[_Mode == KERNEL_DYNAMIC ? 1 : 0]; //!< exit trap (it does not occupy memory if kernel operation mode is not KERNEL_DYNAMIC)
    EFsmState            m_fsm_state;       //!< FSM state

    const EFsmState      m_fsm[FSM_STATE_MAX][FSM_EVENT_MAX] = {
    //    FSM_EVENT_SWITCH     FSM_EVENT_SLEEP     FSM_EVENT_WAKE       FSM_EVENT_EXIT
        { FSM_STATE_SWITCHING, FSM_STATE_SLEEPING, FSM_STATE_NONE,      FSM_STATE_EXITING }, // FSM_STATE_SWITCHING
        { FSM_STATE_NONE,      FSM_STATE_NONE,     FSM_STATE_AWAKENING, FSM_STATE_NONE },    // FSM_STATE_SLEEPING
        { FSM_STATE_SWITCHING, FSM_STATE_SLEEPING, FSM_STATE_NONE,      FSM_STATE_EXITING }, // FSM_STATE_AWAKENING
        { FSM_STATE_NONE,      FSM_STATE_NONE,     FSM_STATE_NONE,      FSM_STATE_NONE }     // FSM_STATE_EXITING
    };
};

} // namespace stk

#endif /* STK_H_ */
