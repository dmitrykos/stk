/*
 * SuperTinyKernel(TM) RTOS: Lightweight High-Performance Deterministic C++ RTOS for Embedded Systems.
 *
 * Source: https://github.com/SuperTinyKernel-RTOS
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>. All Rights Reserved.
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_SYNC_EVENT_FLAGS_H_
#define STK_SYNC_EVENT_FLAGS_H_

#include "stk_sync_cv.h"

/*! \file  stk_sync_eventflags.h
    \brief Implementation of synchronization primitive: stk::sync::EventFlags.
*/

namespace stk {
namespace sync {

/*! \class EventFlags
    \brief 32-bit event flags group for multi-flag synchronization between tasks.

    EventFlags maintains a 32-bit word where each bit represents an independent
    boolean event. Tasks can set or clear any combination of flags, and wait for
    any subset to become set — either requiring \b all requested flags (AND
    semantics) or \b any one of them (OR semantics).

    Unlike \c Event, which models a single binary signal, \c EventFlags allows
    fine-grained coordination across up to 32 independent conditions in a single
    object.

    \note  **Wait semantics**: the caller selects the mode via the \a options
           parameter using \c WAIT_ANY (default) or \c WAIT_ALL.
    \note  **Auto-clear**: by default the matched flags are atomically cleared
           upon a successful \c Wait(). Pass \c NO_CLEAR to suppress this.
    \note  **Direct Handover**: \c Set() notifies \b all waiters so that every
           task re-evaluates its own predicate after waking. Each waiter clears
           only the flags it matched, so concurrent waiters with disjoint
           flag masks do not interfere with each other.
    \note  Maximum flag bit index is 30 (bits 0..30). Bit 31 is reserved for
           future error reporting and must never be set by the caller.

    \code
    // Example: sensor fusion — wait for GPS and IMU data to both arrive
    stk::sync::EventFlags g_SensorFlags;

    static const uint32_t FLAG_GPS = (1U << 0);
    static const uint32_t FLAG_IMU = (1U << 1);

    void ISR_GPS() {
        g_SensorFlags.Set(FLAG_GPS);   // ISR-safe
    }

    void ISR_IMU() {
        g_SensorFlags.Set(FLAG_IMU);   // ISR-safe
    }

    void Task_Fusion() {
        // block until BOTH flags are set simultaneously; clears them on return
        uint32_t raised = g_SensorFlags.Wait(FLAG_GPS | FLAG_IMU,
                                             EventFlags::WAIT_ALL, 5000);
        if (raised & EventFlags::ERROR_TIMEOUT) {
            // handle timeout
        } else {
            // process fused sensor data
        }
    }
    \endcode

    \note  **Rendezvous**: \c Sync() sets flags and waits for a group of flags atomically and releases all
           participants at once (multi-task barrier, equivalent of FreeRTOS xEventGroupSync()).

    \see  Event, Semaphore, ConditionVariable
    \note Only available when kernel is compiled with \a KERNEL_SYNC mode enabled.
*/
class EventFlags final : public ITraceable
{
public:

    // -----------------------------------------------------------------------
    // Options constants (bitmask, passed to Wait())
    // -----------------------------------------------------------------------

    /*! \brief Wait for ANY of the specified flags to be set (OR semantics, default). */
    static const uint32_t OPT_WAIT_ANY = 0x00000000U;

    /*! \brief Wait for ALL of the specified flags to be set simultaneously (AND semantics). */
    static const uint32_t OPT_WAIT_ALL = 0x00000001U;

    /*! \brief Do not clear matched flags after a successful wait. */
    static const uint32_t OPT_NO_CLEAR = 0x00000002U;

    // -----------------------------------------------------------------------
    // Return-value error sentinels (bit 31 is the error indicator)
    // -----------------------------------------------------------------------

    /*! \brief Return sentinel: invalid \a flags argument (bit 31 set). */
    static const uint32_t ERROR_PARAMETER = 0x80000001U;

    /*! \brief Return sentinel: wait timed out before the flags condition was met. */
    static const uint32_t ERROR_TIMEOUT   = 0x80000002U;

    /*! \brief Return sentinel: called from an ISR with a blocking timeout. */
    static const uint32_t ERROR_ISR       = 0x80000004U;

    /*! \brief Return sentinel: the wait was cancelled via IKernel::CancelTaskWait()
               before the flag condition was met (independent of the deadline). */
    static const uint32_t ERROR_CANCELED  = 0x80000008U;

    /*! \brief Return sentinel: the kernel rejected the wait (IKernelService::Wait() returned
               WAIT_RESULT_FAIL, e.g. KERNEL_SYNC mode is not enabled). */
    static const uint32_t ERROR_FAIL      = 0x80000010U;

    /*! \brief Reserved error mask. Any return value with bit 31 set is an error. */
    static const uint32_t ERROR_MASK      = 0x80000000U;

    // -----------------------------------------------------------------------
    // Validation helpers
    // -----------------------------------------------------------------------

    /*! \brief  Checks if a return value from Set(), Clear(), or Wait() is an error.
        \return \c true when the value carries an error sentinel (bit 31 set).
    */
    static bool IsError(uint32_t result) { return ((result & ERROR_MASK) != 0U); }

    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------

    /*! \brief     Constructor.
        \param[in] initial_flags: Initial value of the 32-bit flags word.
                   Only bits 0..30 are meaningful; bit 31 is reserved.
    */
    explicit EventFlags(uint32_t initial_flags = 0U) : m_flags(initial_flags)
    {
        STK_ASSERT((initial_flags & ERROR_MASK) == 0U); // API contract: bit 31 must not be set
    }

    /*! \brief     Destructor.
        \note      If tasks are still waiting at destruction time it is a logical error
                   (dangling waiters). An assertion is triggered in debug builds.
        \note      MISRA deviation: [STK-DEV-005] Rule 10-3-2.
    */
    STK_VIRT_DTOR ~EventFlags() = default;

    // -----------------------------------------------------------------------
    // Flag manipulation
    // -----------------------------------------------------------------------

    /*! \brief     Set one or more flags.
        \details   Performs an atomic OR of \a flags into the internal flags word,
                   then wakes all tasks currently waiting on this object so that
                   each can re-evaluate its own wait condition.
        \param[in] flags: Bitmask of flags to set. Must not have bit 31 set.
        \return    The flags word value \b after setting, or \c ERROR_PARAMETER
                   if \a flags is 0 or has bit 31 set.
        \note      ISR-safe.
    */
    uint32_t Set(uint32_t flags);

    /*! \brief     Clear one or more flags.
        \details   Atomically clears the specified flags from the internal word.
        \param[in] flags: Bitmask of flags to clear. Must not have bit 31 set.
        \return    The flags word value \b before clearing, or \c ERROR_PARAMETER
                   if \a flags is 0 or has bit 31 set.
        \note      ISR-safe.
    */
    uint32_t Clear(uint32_t flags);

    /*! \brief     AND the flags word with a mask (equivalent of ThreadX tx_event_flags_set(TX_AND)).
        \details   Atomically performs <tt>flags &= mask</tt>: every flag bit that is 0 in \a mask is
                   cleared, every flag bit that is 1 in \a mask is preserved. This is the inverse of
                   Clear(), which clears the bits that are 1 in its argument. Any \a mask value is
                   valid, including 0 (clears all flags) and 0xFFFFFFFF (no change).
        \param[in] mask: Bitmask of flags to \b keep.
        \return    The flags word value \b before masking.
        \note      Does not wake waiters: this operation can only remove flags, so it can never
                   satisfy a pending wait condition.
        \note      ISR-safe.
        \see       Clear
    */
    uint32_t And(uint32_t mask);

    /*! \brief     Read the current flags word without modifying it.
        \return    Snapshot of the current 32-bit flags word.
        \note      The returned value is a point-in-time snapshot; it may be stale
                   by the time the caller acts on it.
        \note      ISR-safe.
    */
    uint32_t Get() const;

    // -----------------------------------------------------------------------
    // Waiting
    // -----------------------------------------------------------------------

    /*! \brief     Wait for one or more flags to be set.
        \details   Suspends the calling task until the requested flag condition
                   is satisfied or the timeout expires.

                   On success the matched flags are atomically cleared (unless
                   \c NO_CLEAR is passed in \a options).

        \param[in] flags: Bitmask of flag bits to watch. Must not be 0 and must
                   not have bit 31 set.
        \param[in] options: Combination of \c WAIT_ANY / \c WAIT_ALL and optionally
                   \c NO_CLEAR. Default: \c WAIT_ANY (clear on success).
        \param[in] timeout_ticks: Maximum time to wait (ticks).
                   Use \c WAIT_INFINITE to block indefinitely,
                   \c NO_WAIT for a non-blocking poll.
        \return    Bitmask of the flags that caused the wakeup (the matched subset),
                   or one of the \c ERROR_* sentinels on failure. Always check
                   \c IsError() before using the return value as a flags mask.
        \note      If the predicate becomes satisfied in the same tick that the deadline
                   expires, the wait succeeds and returns the matched flags. The timeout
                   is only reported when the condition is not met at deadline time.
        \note      Returns \c ERROR_CANCELED, not \c ERROR_TIMEOUT, if the wait was
                   interrupted via \c IKernel::CancelTaskWait() before either the flag
                   condition was met or the deadline expired.
        \warning   ISR-safe only with \a timeout_ticks = \c NO_WAIT, ISR-unsafe otherwise.
    */
    uint32_t Wait(uint32_t flags, uint32_t options = OPT_WAIT_ANY, Timeout timeout_ticks = WAIT_INFINITE);

    /*! \brief     Wait for one or more flags to be set, reporting the outcome via EWaitResult.
        \details   Same wait semantics as Wait(), but the status and the flags
                   word are reported separately, so no bit of the flags word is overloaded as an
                   error indicator (equivalent of ThreadX tx_event_flags_get() with \c actual_flags).
        \param[in]  flags: Bitmask of flag bits to watch. Must not be 0 and must not have bit 31 set.
        \param[in]  options: Combination of \c OPT_WAIT_ANY / \c OPT_WAIT_ALL and optionally
                    \c OPT_NO_CLEAR.
        \param[in]  timeout_ticks: Maximum time to wait (ticks), \c WAIT_INFINITE or \c NO_WAIT.
        \param[out] actual_flags: Snapshot of the \b entire flags word taken when the call completed,
                    \b before any auto-clear is applied. On \c WAIT_RESULT_SIGNAL the matched subset
                    is <tt>actual_flags & flags</tt> (WAIT_ANY) or \a flags (WAIT_ALL). On timeout,
                    cancellation or failure it is the current flags word at that moment.
        \return    \c WAIT_RESULT_SIGNAL if the condition was met (matched flags cleared unless
                   \c OPT_NO_CLEAR), otherwise the timeout / \c WAIT_RESULT_CANCELED /
                   \c WAIT_RESULT_FAIL result of the kernel wait.
        \note      Invalid arguments (\a flags is 0 or has bit 31 set) and a blocking wait from an ISR
                   are reported as \c WAIT_RESULT_FAIL, and the wait is not performed. Use the
                   legacy overload if the distinct \c ERROR_PARAMETER / \c ERROR_ISR codes are needed.
        \warning   ISR-safe only with \a timeout_ticks = \c NO_WAIT, ISR-unsafe otherwise.
    */
    EWaitResult WaitEx(uint32_t flags, uint32_t options, Timeout timeout_ticks, uint32_t &actual_flags);

    /*! \brief      Non-blocking flag poll with EWaitResult and full flags snapshot.
        \param[in]  flags: Bitmask of flag bits to watch.
        \param[in]  options: \c OPT_WAIT_ANY or \c OPT_WAIT_ALL, optionally \c OPT_NO_CLEAR.
        \param[out] actual_flags: Snapshot of the entire flags word (see WaitEx()).
        \return     \c WAIT_RESULT_SIGNAL if satisfied, otherwise a non-signal result.
        \note       ISR-safe.
    */
    EWaitResult TryWaitEx(uint32_t flags, uint32_t options, uint32_t &actual_flags)
    {
        return WaitEx(flags, options, NO_WAIT, actual_flags);
    }

    /*! \brief  State of one Sync() rendezvous group, supplied by the caller.
        \details One SyncContext object represents one rendezvous group: it must be shared by (and only
                 by) all tasks which rendezvous together, e.g. a global or static object next to the
                 EventFlags object. It carries the generation counter and the flags snapshot used to
                 release all blocked participants when the rendezvous completes, so EventFlags itself
                 needs no extra state. Independent groups on the same EventFlags object use separate
                 contexts. The object must outlive all calls using it and must not be copied while tasks
                 are blocked on it.
    */
    struct SyncContext
    {
        SyncContext() : m_gen(0U), m_snapshot(0U)
        {}

        uint32_t m_gen;      //!< rendezvous generation, incremented when the rendezvous completes
        uint32_t m_snapshot; //!< flags word captured when the last rendezvous completed
    };

    /*! \brief      Atomically set flags and wait for a group of flags: a multi-task rendezvous
                    (equivalent of FreeRTOS xEventGroupSync()).
        \details    Sets \a set_flags, then blocks until \b all of \a wait_flags are set. The set-then-wait
                    sequence is indivisible and, unlike a Set() followed by Wait(OPT_WAIT_ALL), it releases
                    \b every task blocked in Sync() on the same \a ctx when the last participant arrives:
                    the task which completes the condition clears \a wait_flags once and all blocked
                    participants return success. (With Set() + Wait(OPT_WAIT_ALL) the first task to run
                    would clear the flags and the other participants would never see the condition
                    satisfied again.)
        \param[in,out] ctx: Rendezvous group state shared by all participants of the group (see SyncContext).
        \param[in]  set_flags: Flags to set on entry (this task's "I have arrived" bits). May be 0 (observer
                    role: wait without contributing a bit). Must not have bit 31 set.
        \param[in]  wait_flags: Flags which must all be set to release the group. Must not be 0 and must
                    not have bit 31 set. All participants sharing one \a ctx must pass the same value.
        \param[in]  timeout_ticks: Maximum time to wait (ticks), \c WAIT_INFINITE or \c NO_WAIT.
        \param[out] actual_flags: Snapshot of the \b entire flags word taken when the rendezvous completed,
                    \b before \a wait_flags were cleared. On timeout, cancellation or failure it is the
                    current flags word at that moment.
        \return     \c WAIT_RESULT_SIGNAL if the rendezvous completed, otherwise the timeout /
                    \c WAIT_RESULT_CANCELED / \c WAIT_RESULT_FAIL result. Invalid arguments and a blocking
                    call from an ISR are reported as \c WAIT_RESULT_FAIL and nothing is set.
        \note       Flags set by a task which then times out stay set (they are not rolled back).
        \note       A completion can also be caused by a plain Set() which satisfies \a wait_flags: the next
                    woken Sync() waiter then completes the rendezvous.
        \warning    ISR-safe only with \a timeout_ticks = \c NO_WAIT, ISR-unsafe otherwise.
    */
    EWaitResult Sync(SyncContext &ctx, uint32_t set_flags, uint32_t wait_flags, Timeout timeout_ticks,
        uint32_t &actual_flags);

    /*! \brief     Non-blocking flag poll.
        \details   Checks immediately whether the flag condition is satisfied.
                   Clears matched flags on success unless \c NO_CLEAR is set.
        \param[in] flags: Bitmask of flag bits to watch.
        \param[in] options: \c WAIT_ANY (default) or \c WAIT_ALL, optionally \c NO_CLEAR.
        \return    Matched flags bitmask, or an \c ERROR_* sentinel.
        \note      ISR-safe.
    */
    uint32_t TryWait(uint32_t flags, uint32_t options = OPT_WAIT_ANY)
    {
        return Wait(flags, options, NO_WAIT);
    }

    /*! \brief     Get list of tasks blocked on this object.
        \note      Read-only access for diagnostics / telemetry.
        \note      The result is a snapshot: call inside the critical section (see \a hw::CriticalSection,
                   \a sync::ScopedCrticalSection) if the list may change concurrently.
    */
    const IWaitObject::ListHeadType &GetWaitList() const
    {
        return m_cv.GetWaitList();
    }

    /*! \brief     Get id of the task which is first in the wait list (FIFO order), i.e. the one
                   that WakeOne() would wake next.
        \return    TId of the first waiter, or \a TID_NONE if no tasks are currently waiting.
        \note      Read-only introspection for diagnostics / telemetry.
        \note      The result is a snapshot: call inside the critical section (see \a hw::CriticalSection,
                   \a sync::ScopedCrticalSection) if the list may change concurrently.
        \see       GetWaitList
    */
    TId GetFirstWaiterTid() const
    {
        return m_cv.GetFirstWaiterTid();
    }

private:
    STK_NONCOPYABLE_CLASS(EventFlags);

    // Predicate: returns true when the wait condition for 'flags'/'options' is met.
    // Must be called with m_mutex held.
    bool IsSatisfied(uint32_t flags, uint32_t options) const
    {
        return ((options & OPT_WAIT_ALL) == OPT_WAIT_ALL) ? ((m_flags & flags) == flags) :
            ((m_flags & flags) != 0U);
    }

    // Core wait loop shared by Wait() and WaitEx() (arguments must already be validated).
    // Returns WAIT_RESULT_SIGNAL when the predicate was satisfied, otherwise the failing result.
    // actual_flags receives the whole flags word before auto-clear.
    EWaitResult WaitCore(uint32_t flags, uint32_t options, Timeout timeout_ticks, uint32_t &actual_flags);

    volatile uint32_t m_flags; //!< 32-bit flags word (bit 31 is reserved for errors)
    ConditionVariable m_cv;    //!< woken by Set() to re-evaluate waiting tasks
};

// ---------------------------------------------------------------------------
// Set
// ---------------------------------------------------------------------------

inline uint32_t EventFlags::Set(uint32_t flags)
{
    uint32_t final_result = ERROR_PARAMETER;

    if ((flags != 0U) && ((flags & ERROR_MASK) == 0U))
    {
        const ScopedCriticalSection cs_;

        m_flags |= flags;
        final_result = m_flags;

        // wake all waiters: each task will re-evaluate its own predicate
        m_cv.NotifyAll_CS();
    }

    return final_result;
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

inline uint32_t EventFlags::Clear(uint32_t flags)
{
    uint32_t result = ERROR_PARAMETER;

    if ((flags != 0U) && ((flags & ERROR_MASK) == 0U))
    {
        const ScopedCriticalSection cs_;

        result = m_flags;
        m_flags &= ~flags;
    }

    return result;
}

// ---------------------------------------------------------------------------
// And
// ---------------------------------------------------------------------------

inline uint32_t EventFlags::And(uint32_t mask)
{
    const ScopedCriticalSection cs_;

    const uint32_t result = m_flags;
    m_flags &= mask;

    return result;
}

// ---------------------------------------------------------------------------
// Get
// ---------------------------------------------------------------------------

inline uint32_t EventFlags::Get() const
{
    // atomic on 32-bit aligned targets, a critical section is not required for
    // a single-word read, matching the approach used by Semaphore::GetCount()
    return m_flags;
}

// ---------------------------------------------------------------------------
// Wait
// ---------------------------------------------------------------------------

inline EWaitResult EventFlags::WaitCore(uint32_t flags, uint32_t options,
    Timeout timeout_ticks, uint32_t &actual_flags)
{
    EWaitResult result = WAIT_RESULT_SIGNAL;

    const bool timed_wait = (timeout_ticks != WAIT_INFINITE) && (timeout_ticks != NO_WAIT);

    // capture an absolute deadline once, before entering the wait loop,
    // this prevents the timeout from being silently restarted on each
    // spurious wakeup (e.g. a partial Set() that does not satisfy WAIT_ALL)
    const Timeout deadline = (timed_wait ?
        static_cast<Timeout>(GetTicks() + timeout_ticks) : timeout_ticks);

    ScopedCriticalSection cs_;

    // spin (with blocking) until the predicate is satisfied or we time out
    while (!IsSatisfied(flags, options))
    {
        Timeout remaining = deadline;
        if (timed_wait)
        {
            const Timeout now = static_cast<Timeout>(GetTicks());
            remaining = (now >= deadline ? NO_WAIT : (deadline - now));
        }

        const EWaitResult wr = m_cv.WaitEx(cs_, remaining);

        if (wr != WAIT_RESULT_SIGNAL)
        {
            // timeout (a real deadline miss, or NO_WAIT after 'remaining' collapsed to NO_WAIT),
            // cancellation by IKernel::CancelTaskWait() or kernel failure: stop immediately,
            // do not reinterpret the result and do not keep spinning on the predicate
            result = wr;
            break;
        }
        // else: WAIT_RESULT_SIGNAL - a Set() woke us for re-evaluation (possibly a
        // spurious/partial match under WAIT_ALL); loop back and re-check IsSatisfied()
    }

    // full group snapshot, taken before any auto-clear
    actual_flags = m_flags;

    if (result == WAIT_RESULT_SIGNAL)
    {
        // atomically clear the matched flags unless the caller opted out
        if ((options & OPT_NO_CLEAR) == 0U)
        {
            const uint32_t matched = (((options & OPT_WAIT_ALL) == OPT_WAIT_ALL) ? flags : (m_flags & flags));
            m_flags &= ~matched;
        }
    }

    return result;
}

inline uint32_t EventFlags::Wait(uint32_t flags, uint32_t options, Timeout timeout_ticks)
{
    uint32_t final_result = 0U;

    // validate: flags must be non-zero and must not have the error sentinel bit set
    if ((flags == 0U) || ((flags & ERROR_MASK) != 0U))
    {
        final_result = ERROR_PARAMETER;
    }
    // ISR check: only NO_WAIT is permitted inside an ISR
    else if (hw::IsInsideISR() && (timeout_ticks != NO_WAIT))
    {
        final_result = ERROR_ISR;
    }
    else
    {
        uint32_t actual = 0U;
        const EWaitResult wr = WaitCore(flags, options, timeout_ticks, actual);

        if (wr == WAIT_RESULT_SIGNAL)
        {
            // predicate satisfied: report the matched flags
            final_result = (((options & OPT_WAIT_ALL) == OPT_WAIT_ALL) ? flags : (actual & flags));
        }
        else if (wr == WAIT_RESULT_CANCELED)
        {
            final_result = ERROR_CANCELED;
        }
        else if (wr == WAIT_RESULT_FAIL)
        {
            final_result = ERROR_FAIL;
        }
        else
        {
            final_result = ERROR_TIMEOUT;
        }
    }

    return final_result;
}

inline EWaitResult EventFlags::WaitEx(uint32_t flags, uint32_t options,
    Timeout timeout_ticks, uint32_t &actual_flags)
{
    EWaitResult result = WAIT_RESULT_FAIL;

    if ((flags == 0U) || ((flags & ERROR_MASK) != 0U) ||
        (hw::IsInsideISR() && (timeout_ticks != NO_WAIT)))
    {
        // invalid argument or blocking wait inside an ISR: wait is not performed
        actual_flags = m_flags;
    }
    else
    {
        result = WaitCore(flags, options, timeout_ticks, actual_flags);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Sync
// ---------------------------------------------------------------------------

inline EWaitResult EventFlags::Sync(SyncContext &ctx, uint32_t set_flags, uint32_t wait_flags,
    Timeout timeout_ticks, uint32_t &actual_flags)
{
    EWaitResult result = WAIT_RESULT_FAIL;

    if ((wait_flags == 0U) || ((wait_flags & ERROR_MASK) != 0U) || ((set_flags & ERROR_MASK) != 0U) ||
        (hw::IsInsideISR() && (timeout_ticks != NO_WAIT)))
    {
        // invalid argument or blocking call inside an ISR: nothing is set, nothing is waited for
        actual_flags = m_flags;
    }
    else
    {
        const bool timed_wait = (timeout_ticks != WAIT_INFINITE) && (timeout_ticks != NO_WAIT);

        // absolute deadline captured once (see WaitCore)
        const Timeout deadline = (timed_wait ?
            static_cast<Timeout>(GetTicks() + timeout_ticks) : timeout_ticks);

        ScopedCriticalSection cs_;

        // set this task's bits and remember the rendezvous generation in the same critical section:
        // the set-then-wait sequence is indivisible for every other task
        m_flags |= set_flags;
        const uint32_t gen = ctx.m_gen;

        for (;;)
        {
            if (ctx.m_gen != gen)
            {
                // released by the task which completed the rendezvous: it has already cleared the flags,
                // so the predicate must not be re-evaluated here
                actual_flags = ctx.m_snapshot;
                result = WAIT_RESULT_SIGNAL;
                break;
            }

            if (IsSatisfied(wait_flags, OPT_WAIT_ALL))
            {
                // this task completes the rendezvous (last arriver, or flags satisfied by a plain Set()):
                // publish the snapshot, clear the group flags once and release every blocked participant
                ctx.m_snapshot = m_flags;
                m_flags &= ~wait_flags;
                ++ctx.m_gen;
                m_cv.NotifyAll_CS();

                actual_flags = ctx.m_snapshot;
                result = WAIT_RESULT_SIGNAL;
                break;
            }

            Timeout remaining = deadline;
            if (timed_wait)
            {
                const Timeout now = static_cast<Timeout>(GetTicks());
                remaining = (now >= deadline ? NO_WAIT : (deadline - now));
            }

            const EWaitResult wr = m_cv.WaitEx(cs_, remaining);

            if (wr != WAIT_RESULT_SIGNAL)
            {
                // timeout, cancellation or kernel failure; a completion which raced with the deadline is
                // still honoured: re-check the generation before giving up
                if (ctx.m_gen != gen)
                {
                    actual_flags = ctx.m_snapshot;
                    result = WAIT_RESULT_SIGNAL;
                }
                else
                {
                    actual_flags = m_flags;
                    result = wr;
                }
                break;
            }
            // else: woken for re-evaluation, loop back
        }
    }

    return result;
}

} // namespace sync
} // namespace stk

#endif /* STK_SYNC_EVENT_FLAGS_H_ */
