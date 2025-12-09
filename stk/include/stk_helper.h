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
    uint32_t GetStackSizeBytes() const { return _StackSize * sizeof(size_t); }
    EAccessMode GetAccessMode() const { return _AccessMode; }
    virtual void OnDeadlineMissed(uint32_t duration) { (void)duration; }
    virtual int32_t GetWeight() const { return 1; }

private:
    typename StackMemoryDef<_StackSize>::Type m_stack; //!< memory region
};

/*! \class TaskW
    \brief Partial implementation of the user task. Use for SwitchStrategySmoothWeightedRoundRobin.

    See implementation details and example in Task.
*/
template <int32_t _Weight, uint32_t _StackSize, EAccessMode _AccessMode>
class TaskW : public ITask
{
public:
    enum { STACK_SIZE = _StackSize };
    size_t *GetStack() const { return const_cast<size_t *>(m_stack); }
    uint32_t GetStackSize() const { return _StackSize; }
    uint32_t GetStackSizeBytes() const { return _StackSize * sizeof(size_t); }
    EAccessMode GetAccessMode() const { return _AccessMode; }
    virtual void OnDeadlineMissed(uint32_t duration) { STK_ASSERT(false); /* HRT is unsupported */ }
    virtual int32_t GetWeight() const { return _Weight; }

private:
    typename StackMemoryDef<_StackSize>::Type m_stack; //!< memory region
};

/*! \class StackMemoryWrapper
    \brief Stack memory wrapper for IStackMemory interface.
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
    uint32_t GetStackSizeBytes() const { return _StackSize * sizeof(size_t); }

private:
    MemoryType *m_stack; //!< pointer to the wrapped memory region
};

/*! \brief     Get thread Id.
    \return    Thread Id.
*/
__stk_forceinline size_t GetTid()
{
    return IKernelService::GetInstance()->GetTid();
}

/*! \brief     Get milliseconds from ticks.
    \param[in] ticks: Ticks to convert.
    \param[in] resolution: Resolution (see IKernelService::GetTickResolution).
    \return    Milliseconds.
*/
__stk_forceinline int64_t GetMsecFromTicks(int64_t ticks, int32_t resolution)
{
    return (ticks * resolution) / 1000;
}

/*! \brief     Get ticks from milliseconds.
    \param[in] msec: Milliseconds to convert.
    \param[in] resolution: Resolution (see IKernelService::GetTickResolution).
    \return    Ticks.
*/
__stk_forceinline int64_t GetTicksFromMsec(int64_t msec, int32_t resolution)
{
    return msec * 1000 / resolution;
}

/*! \brief     Get number of ticks elapsed since kernel start.
    \return    Ticks.
*/
__stk_forceinline int64_t GetTicks()
{
    return IKernelService::GetInstance()->GetTicks();
}

/*! \brief     Get number of microseconds in one tick.
    \note      Tick is a periodicity of the system timer expressed in microseconds.
    \return    Microseconds in one tick.
*/
__stk_forceinline int32_t GetTickResolution()
{
    return IKernelService::GetInstance()->GetTickResolution();
}

/*! \brief     Get current time in milliseconds.
    \return    Milliseconds.
*/
__stk_forceinline int64_t GetTimeNowMsec()
{
    IKernelService *service = IKernelService::GetInstance();
    int32_t resolution = service->GetTickResolution();

    if (resolution == 1000)
        return service->GetTicks();
    else
        return (service->GetTicks() * resolution) / 1000;
}

/*! \brief     Delay calling process.
    \note      Unlike Sleep this function delays code execution by spinning in a loop until deadline expiry.
    \note      Use with care in HRT mode to avoid missed deadline (see stk::KERNEL_HRT, ITask::OnDeadlineMissed).
    \param[in] msec: Delay time (milliseconds).
*/
__stk_forceinline void Delay(uint32_t msec)
{
    IKernelService::GetInstance()->Delay(msec);
}

/*! \brief     Put calling process into a sleep state.
    \note      Unlike Delay this function does not waste CPU cycles and allows kernel to put CPU into a low-power state.
    \note      Unsupported in HRT mode (see stk::KERNEL_HRT), instead task will sleep automatically according its periodicity and workload.
    \param[in] msec: Sleep time (milliseconds).
*/
__stk_forceinline void Sleep(uint32_t msec)
{
    IKernelService::GetInstance()->Sleep(msec);
}

/*! \brief     Notify scheduler that it can switch to a next task.
*/
__stk_forceinline void Yield()
{
    IKernelService::GetInstance()->SwitchToNext();
}

} // namespace stk

#endif /* STK_HELPER_H_ */
