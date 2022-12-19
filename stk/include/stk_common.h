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
    KERNEL_STATIC  = 0, //!< All tasks are static and can not exit.
    KERNEL_DYNAMIC      //!< Tasks can be added or removed and therefore exit when done.
};

/*! \enum  EDefault
    \brief Default constants.
*/
enum EDefault
{
    DEFAULT_RESOLUTION_US_ONE_MSEC = 10000, //!< Default resolution (1 millisecond) of the kernel supplied to Kernel::Start().
};

/*! \class StackMemoryDef
    \brief Stack memory type definition.
    \note  This descriptor provides an encapsulated type only on basis of which you can declare
           your memory array variable.

    Usage example:
    \code
    StackMemory<128>::Type my_memory_array;
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

/*! \class IMemoryRegion
    \brief Interface of the memory region.
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
            \param[out] active:  Stack of the task which shall go into Active state (to which context will switch).
        */
        virtual void OnSysTick(Stack **idle, Stack **active) = 0;

        /*! \brief      Called from the Thread process when task finished (its Run function exited by return).
            \param[out] stack: Stack of the exited task.
        */
        virtual void OnTaskExit(Stack *stack) = 0;
    };

    /*! \brief     Start scheduling.
        \param[in] event_handler: Event handler.
        \param[in] resolution_us: Tick resolution in microseconds (for example 1000 equals to 1 millisecond resolution).
        \param[in] first_task: First kernel task which will be called upon start.
        \param[in] main_process: Stack of the main process (optional, if provided then tasks can be dynamic).
        \note      This function never returns!
    */
    virtual void Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *first_task, Stack *main_process) = 0;

    /*! \brief     Initialize stack memory of the user task.
        \param[in] stack: Stack descriptor.
        \param[in] stack_memory: Stack memory.
        \param[in] user_task: User task to which Stack belongs.
    */
    virtual bool InitStack(Stack *stack, IStackMemory *stack_memory, ITask *user_task) = 0;

    /*! \brief     Switches context of the tasks.
    */
    virtual void SwitchContext() = 0;

    /*! \brief     Stop scheduling tasks.
    */
    virtual void StopScheduling() = 0;

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

/*! \class IKernel
    \brief Interface for the implementation of the kernel of the scheduler.
    \note  Mediator design pattern.
*/
class IKernel
{
public:
    /*! \brief     Initialize kernel.
        \param[in] driver: Pointer to the platform driver implementation.
        \param[in] switch_strategy: Pointer to the task switching strategy.
    */
    virtual void Initialize(IPlatform *driver, ITaskSwitchStrategy *switch_strategy) = 0;

    /*! \brief     Add user task.
        \param[in] user_task: Pointer to the user task to add.
    */
    virtual void AddTask(ITask *user_task) = 0;

    /*! \brief     Remove user task.
        \param[in] user_task: Pointer to the user task to remove.
    */
    virtual void RemoveTask(ITask *user_task) = 0;

    /*! \brief     Start kernel.
        \param[in] resolution_us: Resolution of the system tick (SysTick) timer in microseconds, (see IPlatform::GetSysTickResolution).
    */
    virtual void Start(uint32_t resolution_us) = 0;
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
    virtual int32_t GetTicksResolution() const = 0;

    /*! \brief     Get deadline expressed in ticks.
        \param[in] deadline_ms: Deadline in milliseconds.
    */
    int64_t GetDeadlineTicks(int64_t deadline_ms) const
    {
        return GetTicks() + deadline_ms * 1000 / GetTicksResolution();
    }

    /*! \brief     Delay calling user process by spinning in a loop and checking for a deadline expiry.
        \param[in] delay_ms: Delay in milliseconds.
    */
    void DelaySpin(uint32_t delay_ms) const
    {
        int64_t deadline = GetDeadlineTicks(delay_ms);
        while (GetTicks() < deadline)
        {
            __stk_relax_cpu();
        }
    }
};

} // namespace stk

#endif /* STK_COMMON_H_ */
