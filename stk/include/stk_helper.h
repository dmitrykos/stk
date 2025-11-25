/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_HELPER_H_
#define STK_HELPER_H_

#include "stk_common.h"

/*! \file  stk_helper.h
    \brief Contains helper implementations which simplify user-side code.
*/

namespace stk {

/*! \class Task
    \brief Partial implementation of the user task.

    To implement final concrete version of the user task inherit your implementation from this class.

    Usage example:
    \code
    template <stk::EAccessMode _AccessMode>
    class MyTask : public stk::Task<256, _AccessMode>
    {
    public:
        stk::RunFuncT GetFunc() { return &Run; }
        void *GetFuncUserData() { return this; }

    private:
        static void Run(void *user_data)
        {
            ((MyTask *)user_data)->RunInner();
        }

        void RunInner()
        {
            while (true)
            {
                // do dome work here ...
            }
        }
    };

    MyTask<ACCESS_PRIVILEGED> my_task;
    \endcode
*/
template <uint32_t _StackSize, EAccessMode _AccessMode>
class Task : public ITask
{
public:
    enum { STACK_SIZE = _StackSize };
    size_t *GetStack() const { return const_cast<size_t *>(m_stack); }
    uint32_t GetStackSize() const { return _StackSize; }
    EAccessMode GetAccessMode() const { return _AccessMode; }
    virtual void OnDeadlineMissed(uint32_t duration) { (void)duration; }

private:
    typename StackMemoryDef<_StackSize>::Type m_stack; //!< memory region
};

/*! \class StackMemoryWrapper
    \brief Stack memory wrapper into IStackMemory interface.
    \note  Wrapper design pattern.
*/
template <uint32_t _StackSize>
class StackMemoryWrapper : public IStackMemory
{
public:
    /*! \typedef MemoryType
        \brief   Memory type which can be wrapped.
    */
    typedef typename StackMemoryDef<_StackSize>::Type MemoryType;

    /*! \brief Constructor .
    */
    explicit StackMemoryWrapper(MemoryType *stack) : m_stack(stack)
    {
        // note: stack size must be STACK_SIZE_MIN or bigger
        STK_STATIC_ASSERT(_StackSize >= STACK_SIZE_MIN);
    }

    size_t *GetStack() const { return (*m_stack); }
    uint32_t GetStackSize() const { return _StackSize; }

private:
    MemoryType *m_stack; //!< pointer to the wrapped memory region
};

/*! \class Memory
    \brief Memory wrapper into IMemory interface.
    \note  Wrapper design pattern.
*/
template <uint32_t _StackSize>
class Memory : public IMemory
{
public:
    /*! \typedef MemoryType
        \brief   Memory type which can be wrapped.
    */
    typedef typename StackMemoryDef<_StackSize>::Type MemoryType;

    size_t *GetPtr() const { return const_cast<size_t *>(m_memory); }
    uint32_t GetSize() const { return _StackSize; }
    uint32_t GetSizeBytes() const { return GetSize() * sizeof(size_t); }

private:
    MemoryType m_memory; //!< stack memory region
};

/*! \brief     Get milliseconds from ticks.
    \param[in] ticks: Ticks to convert.
    \param[in] resolution: Resolution (see IKernelService::GetTickResolution).
    \return    Milliseconds.
*/
__stk_forceinline int64_t GetMillisecondsFromTicks(int64_t ticks, int32_t resolution)
{
    return (ticks * resolution) / 1000;
}

/*! \brief     Get ticks from milliseconds.
    \param[in] ms: Milliseconds to convert.
    \param[in] resolution: Resolution (see IKernelService::GetTickResolution).
    \return    Ticks.
*/
__stk_forceinline int64_t GetTicksFromMilliseconds(int64_t ms, int32_t resolution)
{
    return ms * 1000 / resolution;
}

/*! \brief     Get current time in milliseconds.
    \return    Milliseconds.
*/
__stk_forceinline int64_t GetTimeNowMilliseconds()
{
    int32_t resolution = Singleton<IKernelService *>::Get()->GetTickResolution();

    if (resolution == 1000)
        return Singleton<IKernelService *>::Get()->GetTicks();
    else
        return (Singleton<IKernelService *>::Get()->GetTicks() * resolution) / 1000;
}

/*! \brief     Delay calling process.
    \note      Unlike Sleep this function delays code execution by spinning in a loop until deadline expiry.
    \note      Use with care in HRT mode to avoid missed deadline (see stk::KERNEL_HRT, ITask::OnDeadlineMissed).
    \param[in] delay_ms: Delay time (milliseconds).
*/
__stk_forceinline void Delay(uint32_t delay_ms)
{
    Singleton<IKernelService *>::Get()->Delay(delay_ms);
}

/*! \brief     Put calling process into a sleep state.
    \note      Unlike Delay this function does not waste CPU cycles and allows kernel to put CPU into a low-power state.
    \note      Unsupported in HRT mode (see stk::KERNEL_HRT), instead task will sleep automatically according its periodicity and workload.
    \param[in] sleep_ms: Sleep time (milliseconds).
*/
__stk_forceinline void Sleep(uint32_t sleep_ms)
{
    Singleton<IKernelService *>::Get()->Sleep(sleep_ms);
}

/*! \brief     Notify scheduler that it can switch to a next task.
*/
__stk_forceinline void Yield()
{
    Singleton<IKernelService *>::Get()->SwitchToNext();
}

} // namespace stk

#endif /* STK_HELPER_H_ */
