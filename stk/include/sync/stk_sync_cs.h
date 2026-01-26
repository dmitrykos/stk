/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_SYNC_CS_H_
#define STK_SYNC_CS_H_

#include "stk_common.h"

/*! \file  stk_sync_cs.h
    \brief Implementation of critical section primitive: ScopedCriticalSection.
*/

namespace stk {
namespace sync {

/*! \class ScopedCriticalSection
    \brief RAII-style low-level synchronization primitive for atomic code execution.

    Enters a critical section upon construction and exits automatically when the
    object goes out of scope. In the context of STK, this typically involves
    disabling interrupts or locking the scheduler.

    \code
    // Example: Protecting a global shared variable
    uint32_t g_SharedCounter = 0;

    void IncrementCounter() {
        {                                    // code execution scope starts here
            stk::ScopedCriticalSection __cs; // critical section starts here
            g_SharedCounter++;               // atomic update
        }                                    // critical section ends here (RAII)
    }
    \endcode

    \note  Global Impact: Use with extreme care. This primitive has a global effect
           on the system by preventing preemption. Long-running code inside a critical
           section will increase interrupt latency and may cause other tasks to miss
           their deadlines.
    \note  Unlike higher-level synchronization primitives, this is always available
           and does not depend on the \a KERNEL_SYNC configuration.
    \see   IMutex, Event, Mutex, Semaphore
*/
class ScopedCriticalSection : private IMutex
{
    friend class Event;
    friend class Mutex;
    friend class Semaphore;

public:
    /*! \brief Enters critical section.
    */
    explicit ScopedCriticalSection() { Lock(); }

    /*! \brief Exits critical section.
    */
    ~ScopedCriticalSection() { Unlock(); }

private:
    void Lock() { stk::EnterCriticalSection(); }
    void Unlock() { stk::ExitCriticalSection(); }
};

} // namespace sync
} // namespace stk

#endif /* STK_SYNC_CS_H_ */
