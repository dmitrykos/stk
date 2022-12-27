/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_COMMON_H_
#define STK_COMMON_H_

#include "stk_defs.h"
#include "stk_linked_list.h"

/*! \file  stk_common.h
    \brief Contains interface definitions of the library.
*/

namespace stk {

/*! \typedef RunFuncType
    \brief   User task main entry function prototype.
*/
typedef void (*RunFuncType) (void *user_data);

/*! \enum  EAccessMode
    \brief Hardware access mode by the user task.
*/
enum EAccessMode
{
    ACCESS_USER = 0,  //!< Unprivileged access mode (access to some hardware is restricted, see CPU manual for details).
    ACCESS_PRIVILEGED //!< Privileged access mode (access to hardware is fully unrestricted).
};

/*! \enum  EKernelMode
    \brief Kernel operating mode.
*/
enum EKernelMode
{
    KERNEL_STATIC  = (1 << 0), //!< All tasks are static and can not exit.
    KERNEL_DYNAMIC = (1 << 1), //!< Tasks can be added or removed and therefore exit when done.
    KERNEL_HRT     = (1 << 2), //!< Hard Real-Time (HRT) behavior (tasks are scheduled periodically and have an execution deadline, whole system is failed when task's deadline is failed).
};

/*! \enum  EStackType
    \brief Stack type.
    \see   IPlatform::InitStack
*/
enum EStackType
{
    STACK_USER_TASK = 0, //!< Stack of the user task.
    STACK_SLEEP_TRAP,    //!< Stack of the Sleep trap.
    STACK_EXIT_TRAP      //!< Stack of the Exit trap.
};

/*! \enum  EConsts
    \brief Constants.
*/
enum EConsts
{
    PERIODICITY_MAX      = 99000, //!< Maximum periodicity (microseconds), 60 seconds (note: this value is the highest working on a real hardware and QEMU).
    PERIODICITY_DEFAULT  = 1000,  //!< Default periodicity (microseconds), 1 millisecond.
    STACK_SIZE_MIN       = 32     //!< Stack memory size of the Exit trap (see: StackMemoryDef, StackMemoryWrapper).
};

/*! \class StackMemoryDef
    \brief Stack memory type definition.
    \note  This descriptor provides an encapsulated type only on basis of which you can declare
           your memory array variable.

    Usage example:
    \code
    StackMemoryDef<128>::Type my_memory_array;
    \endcode
*/
template <uint32_t _StackSize> struct StackMemoryDef
{
    /*! \typedef Type
        \brief   Stack memory type.
    */
    typedef __stk_aligned(16) size_t Type[_StackSize];
};

/*! \class Stack
    \brief Stack descriptor.
*/
struct Stack
{
    size_t SP; //!< Stack Pointer (SP) register
};

/*! \class Singleton
    \brief Provides access to the referenced instance via a single method Singleton<_InstanceType>::Get().
    \note  Singleton design pattern (see Kernel::KernelService for a practical example).

    Usage example:
    \code
    // initialize in a source file:
    class MyClass : private Singleton<MyClass *>
    {
        MyClass()
        {
            Singleton<MyClass *>::Bind(this);
        }
    };

    // initialize in a source file:
    template <> MyClass *Singleton<MyClass *>::m_instance = NULL;

    // use anywhere after MyClass was instantiated
    Singleton<MyClass *>::Get()->SomeUsefulMethod();
    \endcode
*/
template <class _InstanceType> class Singleton
{
public:

    /*! \brief  Get IKernelService instance.
        \note   IKernelService instance is available only after kernel was started with Kernel::Start().
    */
    static __stk_forceinline _InstanceType const &Get() { return m_instance; }

protected:
    /*! \brief     Bind instance to the singleton.
        \param[in] instance: Instance to bind.
    */
    static void Bind(const _InstanceType &instance)
    {
        STK_ASSERT(instance != NULL);
        STK_ASSERT(m_instance == NULL);

        m_instance = instance;
    }

#ifdef _STK_UNDER_TEST
    /*! \brief     Unbind instance from the singleton.
        \note      It is only enabled when STK is under a test, should not be in production.
        \param[in] instance: Instance to unbind.
    */
    static void Unbind(const _InstanceType &instance)
    {
        STK_ASSERT(instance != NULL);

        // allow if not bound
        if (m_instance == NULL)
            return;

        STK_ASSERT(m_instance == instance);
        m_instance = NULL;
    }
#endif

private:
    static _InstanceType m_instance; //!< referenced single instance
};

/*! \class IStackMemory
    \brief Interface of the stack memory region.
*/
class IStackMemory
{
public:
    /*! \brief Get pointer to the stack memory.
    */
    virtual size_t *GetStack() = 0;

    /*! \brief Get size of the stack memory array (number of size_t elements in the array).
    */
    virtual uint32_t GetStackSize() const = 0;
};

/*! \class ITask
    \brief Interface of the user task.

    Kernel task hosts user task.

    Usage example:
    \code
    Task<ACCESS_USER> task1(0);
    Task<ACCESS_USER> task2(1);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    \endcode
*/
class ITask : public IStackMemory
{
public:
    /*! \brief     Get user task's main entry function.
    */
    virtual RunFuncType GetFunc() = 0;

    /*! \brief     Get user data which is supplied to the user task's main entry function.
    */
    virtual void *GetFuncUserData() = 0;

    /*! \brief     Get hardware access mode of the user task.
    */
    virtual EAccessMode GetAccessMode() const = 0;

    /*! \brief     Called by a scheduler if deadline of the task is missed when Kernel is operating in Hard Real-Time mode (see stk::KERNEL_HRT).
        \param[in] duration: Actual duration value which will always be larger than deadline value which was missed.
        \note      Optional handler. Use it for example for logging of the faulty task.
    */
    virtual void OnDeadlineMissed(uint32_t duration) = 0;
};

/*! \class IKernelTask
    \brief Interface of the kernel task.

    Kernel task hosts user task.
*/
class IKernelTask : public util::DListEntry<IKernelTask>
{
public:
    /*! \typedef   ListHeadType
        \brief     List head type for IKernelTask elements.
    */
    typedef util::DListHead<IKernelTask> ListHeadType;

    /*! \typedef   ListEntryType
        \brief     List entry type of IKernelTask elements.
    */
    typedef util::DListEntry<IKernelTask> ListEntryType;

    /*! \brief     Get user task.
    */
    virtual ITask *GetUserTask() = 0;

    /*! \brief     Get pointer to the user task's stack.
    */
    virtual Stack *GetUserStack() = 0;
};

/*! \class IPlatform
    \brief Interface of the platform driver.
    \note  Bridge design pattern. Do not put implementation details in the header of the
           concrete class to avoid breaking this pattern.

    Platform driver represents an underlying hardware and implements the following logic:
     - time tick
     - context switching
     - hardware access from the user task

    All functions are called by the kernel implementation.
*/
class IPlatform
{
public:
    /*! \class IEventHandler
        \brief Interface of the back-end event handler.

        It is inherited by the kernel implementation and delivers events from ISR.
    */
    class IEventHandler
    {
    public:
        /*! \brief     Called by ISR handler to notify that scheduling is about to start.
            \note      This event can be used to change hardware access mode for the first task.
        */
        virtual void OnStart() = 0;

        /*! \brief      Called by ISR handler to notify about the next system tick.
            \param[out] idle: Stack of the task which shall go into Idle state.
            \param[out] active: Stack of the task which shall go into Active state (to which context will switch).
        */
        virtual void OnSysTick(Stack **idle, Stack **active) = 0;

        /*! \brief      Called by Thread process (via IKernelService::SwitchToNext) to switch to a next task.
            \param[in]  caller_SP: Value of Stack Pointer (SP) register (for locating the calling process inside the kernel).
        */
        virtual void OnTaskSwitch(size_t caller_SP) = 0;

        /*! \brief      Called by Thread process (via IKernelService::Sleep) for exclusion of the calling process from scheduling (sleeping).
            \param[in]  caller_SP: Value of Stack Pointer (SP) register (for locating the calling process inside the kernel).
            \param[in]  sleep_ticks: Time to sleep (ticks).
        */
        virtual void OnTaskSleep(size_t caller_SP, uint32_t sleep_ticks) = 0;

        /*! \brief      Called from the Thread process when task finished (its Run function exited by return).
            \param[out] stack: Stack of the exited task.
        */
        virtual void OnTaskExit(Stack *stack) = 0;
    };

    /*! \brief     Start scheduling.
        \param[in] event_handler: Event handler.
        \param[in] resolution_us: Tick resolution in microseconds (for example 1000 equals to 1 millisecond resolution).
        \param[in] first_task: First kernel task which will be called upon start.
        \param[in] exit_trap: Stack of the Exit trap (optional, provided if kernel is operating in KERNEL_DYNAMIC mode).
        \note      This function never returns!
    */
    virtual void Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task, Stack *exit_trap) = 0;

    /*! \brief     Stop scheduling.
    */
    virtual void Stop() = 0;

    /*! \brief     Initialize stack memory of the user task.
        \param[in] stack_type: Stack type.
        \param[in] stack: Stack descriptor.
        \param[in] stack_memory: Stack memory.
        \param[in] user_task: User task to which Stack belongs.
    */
    virtual bool InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task) = 0;

    /*! \brief     Switches context of the tasks.
    */
    virtual void SwitchContext() = 0;

    /*! \brief     Get resolution of the system tick timer in microseconds.
                   Resolution means a number of microseconds between system tick timer ISRs.
        \return    Microseconds.
    */
    virtual int32_t GetTickResolution() const = 0;

    /*! \brief     Set hardware access mode for the Thread mode.
        \note      In case of Arm processor see its manual (Processor mode and privilege levels for software execution).
        \param[in] mode: Access mode.
    */
    virtual void SetAccessMode(EAccessMode mode) = 0;

    /*! \brief     Switch to a next task.
    */
    virtual void SwitchToNext() = 0;

    /*! \brief     Put calling process into a sleep state.
        \note      Unlike Delay this function does not waste CPU cycles and allows kernel to put CPU into a low-power state.
        \param[in] ticks: Time to sleep (ticks).
    */
    virtual void SleepTicks(uint32_t ticks) = 0;

    /*! \brief     Cause a hard fault of the system.
        \note      Normally called by the Kernel when one of the scheduled tasks missed its deadline (see stk::KERNEL_HRT).
    */
    virtual void HardFault() = 0;
};

/*! \class ITaskSwitchStrategy
    \brief Interface for a task switching strategy implementation.
    \note  Strategy and Iterator design patterns.

    Inherit this interface by your concrete implementation of the task switching strategy.
*/
class ITaskSwitchStrategy
{
public:
    /*! \brief     Add task.
        \note      Kernel tasks are added by the concrete implementation of IKernel.
        \param[in] task: Pointer to the task to add.
    */
    virtual void AddTask(IKernelTask *task) = 0;

    /*! \brief     Remove task.
        \note      Kernel tasks are removed from the concrete implementation of IKernel.
        \param[in] task: Pointer to the task to remove.
    */
    virtual void RemoveTask(IKernelTask *task) = 0;

    /*! \brief     Get first task.
    */
    virtual IKernelTask *GetFirst() = 0;

    /*! \brief     Get next linked task.
        \param[in] current: Pointer to the current task.
        \return    Pointer to the next task.
        \note      Some implementations may return NULL that denotes the end of the iteration.
    */
    virtual IKernelTask *GetNext(IKernelTask *current) = 0;
};

/*! \class IKernelEventHandler
    \brief Interface of the kernel event handler.
    \note  Optional. Can be used to extend functionality of default IPlatform driver handlers.
*/
class IKernelEventHandler
{
public:
    /*! \brief      Called by Kernel when its entering a sleep mode.
        \return     True if event is handled otherwise False to let driver handle it.
    */
    virtual bool OnSleep() = 0;

    /*! \brief      Called by Kernel when hard fault happens.
        \note       Normally called by the Kernel when one of the scheduled tasks missed its deadline (see stk::KERNEL_HRT, IPlatform::HardFault).
        \return     True if event is handled otherwise False to let driver handle it.
    */
    virtual bool OnHardFault() = 0;
};

/*! \class IKernel
    \brief Interface for the implementation of the kernel of the scheduler. It supports Soft and Hard Real-Time modes.
    \note  Mediator design pattern.
*/
class IKernel
{
public:
    /*! \brief     Initialize kernel.
        \param[in] driver: Driver implementation.
        \param[in] switch_strategy: Task switching strategy.
        \param[in] event_handler: Kernel events handler (optional, can be NULL).
    */
    virtual void Initialize(IPlatform *driver, ITaskSwitchStrategy *switch_strategy, IKernelEventHandler *event_handler = NULL) = 0;

    /*! \brief     Add user task.
        \note      This function is for Soft Real-time modes only, e.g. stk::KERNEL_HRT is not used as parameter.
        \param[in] user_task: Pointer to the user task to add.
    */
    virtual void AddTask(ITask *user_task) = 0;

    /*! \brief     Add user task.
        \note      This function is for Hard Real-time mode only, e.g. stk::KERNEL_HRT is used as parameter.
        \param[in] user_task: Pointer to the user task to add.
        \param[in] periodicity_ticks: Periodicity time at which task is scheduled.
        \param[in] deadline_ticks: Deadline time within which a task must complete its work.
        \param[in] start_delay_ticks: Initial start delay for the task.
    */
    virtual void AddTask(ITask *user_task, uint32_t periodicity_ticks, uint32_t deadline_ticks, uint32_t start_delay_ticks) = 0;

    /*! \brief     Remove user task.
        \param[in] user_task: Pointer to the user task to remove.
    */
    virtual void RemoveTask(ITask *user_task) = 0;

    /*! \brief     Start kernel.
        \param[in] resolution_us: Resolution of the system tick (SysTick) timer in microseconds, (see IPlatform::GetSysTickResolution).
        \note      If running on STM32 device with HAL driver or on QEMU do not change the default resolution (PERIODICITY_DEFAULT).
                   STM32's HAL expects 1 millisecond resolution and QEMU does not have enough resolution on Windows
                   platform to operate correctly on a sub-millisecond resolution.
    */
    virtual void Start(uint32_t resolution_us = PERIODICITY_DEFAULT) = 0;
};

/*! \class IKernelService
    \brief Interface for the kernel services exposed to the user processes during
           run-time when Kernel started scheduling the processes.
    \note  State design pattern.
*/
class IKernelService
{
public:
    /*! \brief     Get number of ticks elapsed since the start of the kernel.
    */
    virtual int64_t GetTicks() const = 0;

    /*! \brief     Get number of microseconds in one tick.
        \note      Tick is a periodicity of the system timer expressed in microseconds.
    */
    virtual int32_t GetTickResolution() const = 0;

    /*! \brief     Delay calling process.
        \note      Unlike Sleep this function delays code execution by spinning in a loop until deadline expiry.
        \note      Use with care in HRT mode to avoid missed deadline (see stk::KERNEL_HRT, ITask::OnDeadlineMissed).
        \param[in] delay_ms: Delay time (milliseconds).
    */
    virtual void Delay(uint32_t delay_ms) const = 0;

    /*! \brief     Put calling process into a sleep state.
        \note      Unlike Delay this function does not waste CPU cycles and allows kernel to put CPU into a low-power state.
        \note      Unsupported in HRT mode (see stk::KERNEL_HRT), instead task will sleep automatically according its periodicity and workload.
        \param[in] sleep_ms: Sleep time (milliseconds).
    */
    virtual void Sleep(uint32_t sleep_ms) = 0;

    /*! \brief     Notify scheduler that it can switch to a next task.
    */
    virtual void SwitchToNext() = 0;
};

} // namespace stk

#endif /* STK_COMMON_H_ */
