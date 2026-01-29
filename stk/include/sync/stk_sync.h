/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_SYNC_H_
#define STK_SYNC_H_

/*! \file  stk_sync.h
    \brief Implementation of synchronization primitives.
*/

/*! \namespace stk::sync
    \brief     Synchronization primitives for task coordination and resource protection.

    ISR SAFETY GUIDELINES:
    ------------------------------------------------------------------------------------
    Special care must be taken when calling synchronization  methods from within an Interrupt Service Routine (ISR).

    As a general rule, methods that can cause the caller to block or sleep are **STRICTLY FORBIDDEN** in ISRs.

    | Primitive             | ISR Safe Methods                                        |
    | :-------------------- | :------------------------------------------------------ |
    | **Event**             | \c Set(), \c Pulse(), \c Reset(), \c TryWait()          |
    | **Semaphore**         | \c Signal()                                             |
    | **SpinLock**          | None                                                    |
    | **Mutex**             | None                                                    |
    | **ConditionVariable** | \c NotifyOne(), \c NotifyAll(), \c Wait(NO_WAIT)        |
    | **Pipe**              | None                                                    |

    NOTE:
    * **SpinLock**, **Mutex**: Ownership is tied to a Task ID (\a TId). Since ISRs lack a valid
      Task ID context, Mutex operations are never safe in ISRs.
    * **Pipe** uses internal Mutexes for state protection and is therefore not suitable for direct use in ISRs.
    * **ConditionVariable::Notify** methods are safe as they only trigger internal wake-ups without
      blocking the caller.

    WARNING:
    Calling a blocking method from an ISR will lead to undefined behavior, memory corruption, or a deadlock.
    In debug build STK_ASSERT will break code execution if ineligible for ISR method is called.
*/
namespace stk {
namespace sync {
} // namespace sync
} // namespace stk

#include "stk_sync_cs.h"
#include "stk_sync_cv.h"
#include "stk_sync_event.h"
#include "stk_sync_spinlock.h"
#include "stk_sync_mutex.h"
#include "stk_sync_semaphore.h"
#include "stk_sync_pipe.h"

#endif /* STK_SYNC_H_ */
