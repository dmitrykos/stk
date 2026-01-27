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

#include "stk_sync_cs.h"
#include "stk_sync_cv.h"
#include "stk_sync_event.h"
#include "stk_sync_mutex.h"
#include "stk_sync_semaphore.h"
#include "stk_sync_pipe.h"

/*! \namespace stk::sync
    \brief     Synchronization primitives for task coordination and resource protection.

    ISR SAFETY GUIDELINES:
    ------------------------------------------------------------------------------------
    In the STK environment, special care must be taken when calling synchronization
    methods from within an Interrupt Service Routine (ISR).

    As a general rule, methods that can cause the caller to block or sleep
    are **STRICTLY FORBIDDEN** in ISRs.

    | Primitive             | ISR Safe Methods                 | Non-ISR Safe (Blocking)                |
    | :-------------------- | :------------------------------- | :------------------------------------- |
    | **Event**             | \c Set(), \c Pulse(), \c Reset() | \c Wait()                              |
    | **Semaphore**         | \c Signal()                      | \c Wait()                              |
    | **Mutex**             | None                             | \c Lock(), \c TimedLock(), \c Unlock() |
    | **ConditionVariable** | \c NotifyOne(), \c NotifyAll()   | \c Wait()                              |
    | **Pipe**              | None                             | \c Read(), \c Write(), \c Bulk ops     |

    NOTE:
    * Mutex ownership is tied to a Task ID (\a TId). Since ISRs do not have a
      valid Task ID context, Mutex operations are never safe in ISRs.
    * Pipe uses internal Mutexes for state protection and is therefore
      not suitable for direct use in ISRs.
    * ConditionVariable::Notify methods are safe as they only trigger
      internal wake-ups without blocking the caller.

    WARNING:
    Calling a blocking method from an ISR will lead to undefined behavior,
    memory corruption, or a deadlock.
*/
namespace stk {
namespace sync {
} // namespace sync
} // namespace stk

#endif /* STK_SYNC_H_ */
