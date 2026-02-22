/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_SYNC_RWMUTEX_H_
#define STK_SYNC_RWMUTEX_H_

#include "stk_common.h"
#include "stk_sync_cv.h"
#include "stk_sync_mutex.h"

/*! \file  stk_sync_rwmutex.h
    \brief Implementation of synchronization primitive: Read-Write Mutex.
*/

namespace stk {
namespace sync {

/*! \class RWMutex
    \brief Reader-Writer Lock synchronization primitive for shared and exclusive access.

    RWMutex allows multiple tasks to read a shared resource simultaneously (shared access)
    while ensuring that only one task can write to the resource at a time (exclusive access).
    This is particularly efficient for data structures that are read frequently but
    modified infrequently.

    \note  **Writer Preference Policy**: To prevent "writer starvation," this implementation
           prioritizes waiting writers. If a writer is waiting for the lock, new readers
           will be blocked until the writer has completed its operation.

    \code
    // Example: Protecting app settings
    stk::sync::RWMutex g_SettingsLock;
    Settings           g_Settings;

    void Engine_Task() {
        // Multiple engine instances can read settings concurrently
        stk::sync::RWMutex::ScopedTimedReadMutex guard(g_SettingsLock);
        Apply(g_Settings);
    }

    void UI_Control_Task() {
        // Exclusive access to update settings
        g_SettingsLock.Lock();
        g_Settings.volume = new_volume;
        g_SettingsLock.Unlock();
    }
    \endcode

    \see   Mutex, ConditionVariable, ScopedReadMutex
*/
class RWMutex : public IMutex
{
public:
    /*! \brief     Constructor.
    */
    explicit RWMutex() : m_readers(0), m_writers_waiting(0), m_writer_active(false)
    {}

    /*! \class ScopedTimedLock
        \brief RAII wrapper for attempting exclusive write access with a timeout.
    */
    class ScopedTimedLock
    {
    public:
        /*! \brief  Constructs the guard and attempts to acquire the write lock.
            \param[in] rw: Reference to the RWMutex.
            \param[in] timeout: Maximum time to wait (ticks). Use NO_WAIT for non-blocking (TryLock).
        */
        explicit ScopedTimedLock(RWMutex &rw, Timeout timeout = WAIT_INFINITE)
            : m_rw(rw), m_locked(rw.TimedLock(timeout))
        {}
        ~ScopedTimedLock() { if (m_locked) m_rw.Unlock(); }

        bool IsLocked() const { return m_locked; }

    private:
        RWMutex &m_rw;
        bool     m_locked;
    };

    /*! \class ScopedTimedReadMutex
        \brief RAII wrapper for attempting shared read access with a timeout.
    */
    class ScopedTimedReadMutex {
    public:
        /*! \brief  Constructs the guard and attempts to acquire the read lock.
            \param[in] rw: Reference to the RWMutex.
            \param[in] timeout: Maximum time to wait (ticks). Use NO_WAIT for non-blocking (TryReadLock).
        */
        explicit ScopedTimedReadMutex(RWMutex &rw, Timeout timeout = WAIT_INFINITE)
            : m_rw(rw), m_locked(rw.TimedReadLock(timeout))
        {}
        ~ScopedTimedReadMutex() { if (m_locked) m_rw.ReadUnlock(); }

        bool IsLocked() const { return m_locked; }

    private:
        RWMutex &m_rw;
        bool     m_locked;
    };

    /*! \brief     Acquire the lock for shared reading with a timeout.
        \param[in] timeout: Maximum time to wait (ticks).
        \return    True if lock acquired, false if timeout occurred.
        \warning   ISR-unsafe.
    */
    bool TimedReadLock(Timeout timeout);

    /*! \brief     Acquire the lock for shared reading.
        \details   Blocks the calling task if a writer is currently active or if there
                   are writers waiting to acquire the lock.
        \warning   ISR-unsafe.
    */
    void ReadLock() { (void)TimedReadLock(WAIT_INFINITE); }

    /*! \brief     Attempt to acquire the lock for shared reading without blocking.
        \details   Checks if a writer is active or waiting. If the resource is available
                   for reading, it increments the reader count and returns immediately.
        \return    True if the read lock was acquired, false if a writer is active or waiting.
        \warning   ISR-unsafe.
    */
    bool TryReadLock() { return TimedReadLock(NO_WAIT); }

    /*! \brief     Release the shared reader lock.
        \details   Decrements the reader count. If this was the last active reader,
                   it will notify any waiting writers.
        \warning   ISR-unsafe.
    */
    void ReadUnlock();

    /*! \brief     Acquire the lock for exclusive writing with a timeout.
        \param[in] timeout: Maximum time to wait (ticks).
        \return    True if lock acquired, false if timeout occurred.
        \warning   ISR-unsafe.
    */
    bool TimedLock(Timeout timeout);

    /*! \brief     Acquire the lock for exclusive writing (IMutex interface).
        \details   Blocks the calling task until all active readers have released their
                   locks and no other writer is active.
        \warning   ISR-unsafe.
    */
    void Lock() { (void)TimedLock(WAIT_INFINITE); }

    /*! \brief     Attempt to acquire the lock for exclusive writing without blocking.
        \details   Checks if any readers are active or if another writer is active.
        \return    True if the exclusive lock was acquired, false otherwise.
        \warning   ISR-unsafe.
    */
    bool TryLock() { return TimedLock(NO_WAIT); }

    /*! \brief     Release the exclusive writer lock (IMutex interface).
        \details   Releases the lock and prioritizes waking waiting writers. If no
                   writers are waiting, it wakes all waiting readers.
        \warning   ISR-unsafe.
    */
    void Unlock();

private:
    Mutex             m_mutex;           //!< protects internal state
    ConditionVariable m_cv_readers;      //!< signaled when readers can proceed
    ConditionVariable m_cv_writers;      //!< signaled when a writer can proceed
    uint32_t          m_readers;         //!< current active readers
    uint32_t          m_writers_waiting; //!< count of writers waiting for access
    bool              m_writer_active;   //!< true if a writer currently holds the lock
};

inline bool RWMutex::TimedReadLock(Timeout timeout)
{
    // not supported inside ISR
    STK_ASSERT(!hw::IsInsideISR());

    Mutex::ScopedLock guard(m_mutex);

    // wait if there is an active writer or if writers are waiting
    while (m_writer_active || (m_writers_waiting > 0))
    {
        if (!m_cv_readers.Wait(m_mutex, timeout))
            return false; // timeout

        // after waking, the loop re-checks the condition because another writer
        // might have queued up in the meantime (Writer Preference).
    }

    ++m_readers;
    return true;
}


inline void RWMutex::ReadUnlock()
{
    // mutex operations are not supported inside ISR
    STK_ASSERT(!hw::IsInsideISR());

    Mutex::ScopedLock guard(m_mutex);

    // wake a waiting writer if this was the last reader
    if (--m_readers == 0)
        m_cv_writers.NotifyOne();
}

inline bool RWMutex::TimedLock(Timeout timeout)
{
    // mutex operations are not supported inside ISR
    STK_ASSERT(!hw::IsInsideISR());

    Mutex::ScopedLock guard(m_mutex);

    ++m_writers_waiting;

    // wait until there are no active readers and no active writer
    while (m_writer_active || m_readers > 0)
    {
        if (!m_cv_writers.Wait(m_mutex, timeout))
        {
            // decrement waiting count on timeout and return
            --m_writers_waiting;
            return false;
        }
    }

    --m_writers_waiting;
    m_writer_active = true;
    return true;
}

inline void RWMutex::Unlock()
{
    // mutex operations are not supported inside ISR
    STK_ASSERT(!hw::IsInsideISR());

    Mutex::ScopedLock guard(m_mutex);

    m_writer_active = false;

    // prioritize waking waiting writers, otherwise wake all readers
    if (m_writers_waiting > 0)
        m_cv_writers.NotifyOne();
    else
        m_cv_readers.NotifyAll();
}

} // namespace sync
} // namespace stk

#endif /* STK_SYNC_RWMUTEX_H_ */
