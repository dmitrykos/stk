/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
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
    size_t *GetStack() { return m_stack; }
    uint32_t GetStackSize() const { return _StackSize; }
    EAccessMode GetAccessMode() const { return _AccessMode; }

private:
    typename StackMemoryDef<_StackSize>::Type m_stack; //!< memory region
};

/*! \class StackMemory
    \brief Stack memory.
*/
template <uint32_t _StackSize>
class StackMemory : public IStackMemory
{
public:
    /*! \brief Get pointer to the stack memory.
    */
    size_t *GetStack() { return m_stack; }

    /*! \brief Get size of the stack memory array (number of size_t elements in the array).
    */
    uint32_t GetStackSize() const { return _StackSize; }

private:
    typename StackMemoryDef<_StackSize>::Type m_stack; //!< memory region
};

} // namespace stk

#endif /* STK_HELPER_H_ */
