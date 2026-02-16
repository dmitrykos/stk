# Semaphore Test Suite

Test suite for `stk::sync::Semaphore`.  
Source: `test/semaphore/test_semaphore.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_semaphore.h>
```

`Semaphore` is a counting semaphore that maintains an internal resource counter.
Unlike `ConditionVariable` or `Event`, it is **stateful**: a `Signal()` posted when
no task is waiting increments the counter so the signal is not lost — a subsequent
`Wait()` will drain it on the fast path without ever blocking.

This implementation uses a **Direct Handover** policy: when tasks are waiting,
`Signal()` passes the resource token directly to the first waiter without touching
the counter. The woken task owns the token upon return from `Wait()` without needing
to decrement `m_count` itself.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::Semaphore g_Sem;        // initial count = 0 (default)
stk::sync::Semaphore g_Sem(3);     // initial count = 3 (3 permits immediately available)
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Wait` | `bool Wait(Timeout timeout = WAIT_INFINITE)` | Decrements counter if `> 0` (fast path, no blocking). If counter is `0` and `timeout == 0` (`NO_WAIT`), returns `false` immediately. Otherwise blocks until `Signal()` or timeout. Returns `true` if acquired, `false` on timeout. ISR-unsafe. |
| `Signal` | `void Signal()` | If tasks are waiting, delivers token directly to the first waiter (Direct Handover — counter is not incremented). If no tasks are waiting, increments `m_count`. ISR-safe. |
| `GetCount` | `uint32_t GetCount() const` | Returns the current counter value. ISR-safe. |
| `~Semaphore` | (destructor) | Asserts `m_wait_list.IsEmpty()` in debug builds. Destroying a semaphore with active waiters is a logic error. |

**Fast path vs slow path:**

```
Wait() — m_count > 0  →  decrement m_count, return true immediately (no blocking)
Wait() — m_count == 0, timeout == 0  →  return false immediately (NO_WAIT)
Wait() — m_count == 0, timeout > 0  →  block until Signal() or timeout

Signal() — waiters present  →  wake first waiter directly (m_count unchanged)
Signal() — no waiters       →  ++m_count (signal remembered for future Wait())
```

**Key invariants:**

- Every `Signal()` produces exactly one successful `Wait()` — signals are never lost.
- The counter value plus the number of tasks currently blocked equals the total
  number of signals posted minus the total number of successful acquisitions.
- Direct Handover means `m_count` is never incremented when a waiter is present;
  the waking task does not decrement it either — ownership is transferred implicitly.
- Destroying a `Semaphore` while tasks are waiting is a logic error; an assertion
  fires in debug builds.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_SEM_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_SEM_TEST_TIMEOUT` | `1000` ticks | Blocking timeout for `Wait()` calls that must succeed |
| `_STK_SEM_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_SEM_TEST_LONG_SLEEP` | `100` ticks | Sleep used by the verifier task to wait for workers |
| `_STK_SEM_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestSemaphore` is **reconstructed in-place** via placement-new inside
`ResetTestState()` before each test. `ResetTestState()` accepts an `initial_count`
parameter forwarded to the `Semaphore` constructor, allowing each test to start with
the exact counter value it requires.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eight
distinct task class templates simultaneously. Tests 1–7 are skipped on M0 and only
`StressTest` (test 8) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory. Tests 1–7
each introduce a distinct task class template, multiplying the static memory footprint
beyond what M0 can accommodate.

| Platform | `_STK_SEM_STACK_SIZE` |
|----------|-----------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `BasicSignalWait`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 100` &nbsp;|&nbsp; **Initial count:** `0`

Task 0 is the producer; tasks 1–4 are consumers. The producer fires
`iterations × 4 = 400` signals total, pacing each with `Delay(1)`. Each of the four
consumers loops `Wait()` for `iterations = 100` cycles, incrementing `g_SharedCounter`
on each acquisition. Verifies that every signal produces exactly one successful `Wait()`
with no lost or duplicate deliveries.

**Pass condition:** `counter == 400`

---

### Test 2 — `InitialCount`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Initial count:** `4` (`_STK_SEM_TEST_TASKS_MAX - 1`)

The semaphore is pre-loaded with 4 permits. Tasks 1–4 each call
`Wait(_STK_SEM_TEST_SHORT_SLEEP)` immediately; all four must succeed on the fast path
(counter drains from 4 to 0). Task 0 sleeps briefly to let the drain complete, then
calls `Wait(_STK_SEM_TEST_SHORT_SLEEP)` itself. With the counter at 0 and no `Signal()`
forthcoming, task 0's `Wait()` must time out and return `false`. Verifies the
`initial_count` constructor parameter and the transition from fast-path to blocking.

**Pass condition:** `counter == 4` (all four consumers fast-pathed) and `acquired == false` (task 0 timed out)

---

### Test 3 — `TimeoutWait`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Initial count:** `0`

Task 1 calls `Wait(50)` after `_STK_SEM_TEST_SHORT_SLEEP`; the semaphore stays at zero
so it must time out and return `false` with elapsed time in `[45, 60]` ms. Task 0
withholds `Signal()` until tick 200. Task 2 sleeps until tick 210 then calls
`Wait(100)`, which must succeed on the now-signaled semaphore. Task 2 is the verifier.

**Pass condition:** `counter == 2`
(1 = timeout fired correctly within bounds; 2 = `Wait()` succeeded after `Signal()`)

---

### Test 4 — `ZeroTimeout`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Initial count:** `0`

Verifies `Wait(NO_WAIT)` (timeout == 0) non-blocking behavior on both paths. Task 1
calls `Wait(NO_WAIT)` on a zero-count semaphore; must return `false` immediately
(elapsed < `_STK_SEM_TEST_SHORT_SLEEP`). Task 2 calls `Signal()` to add one permit,
then `Wait(NO_WAIT)`; must return `true` immediately as the fast path takes effect.
Both checks contribute one increment each to `g_SharedCounter`.

**Pass condition:** `counter == 2`
(1 = `Wait(NO_WAIT)` on empty returned `false` immediately; 2 = `Wait(NO_WAIT)` on pre-signaled returned `true` immediately)

---

### Test 5 — `SignalBeforeWait`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Param:** `iterations = 200` &nbsp;|&nbsp; **Initial count:** `0`

Verifies the semaphore's stateful "signal remembered" property. Task 0 calls
`Signal()` 200 times consecutively before any consumer reaches `Wait()`, then reads
`GetCount()` to confirm the counter accumulated to 200. Task 1 then sleeps briefly to
ensure all signals are posted, and drains all 200 permits by calling `Wait()` 200
times — every call must fast-path (no blocking). Verifies that signals posted with no
waiters are never discarded.

**Pass condition:** `count_after_signals == 200` and `counter == 200`

---

### Test 6 — `FIFOOrder`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Initial count:** `0`

Verifies that blocked tasks are woken in FIFO arrival order. Tasks 1–4 stagger their
entry into `Wait()` by sleeping `SHORT_SLEEP × m_task_id` ticks, ensuring they queue
in ascending task-id order (1, 2, 3, 4). Task 0 waits 50 ticks for all consumers to
block, then calls `Signal()` four times with `Delay(1)` between each. As each
consumer wakes it records its `m_task_id` into `g_AcquisitionOrder[g_OrderIndex++]`.
The verifier checks the array is exactly `[1, 2, 3, 4]`.

**Pass condition:** `g_AcquisitionOrder == [1, 2, 3, 4]`

---

### Test 7 — `BoundedBuffer`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Param:** `iterations = 200` &nbsp;|&nbsp; **Initial count:** `0`

Models the classic single-producer / single-consumer bounded-buffer pattern. Task 0
(producer) calls `Delay(1)` then `Signal()` in a 200-iteration loop, simulating work
before each item is made available. Task 1 (consumer) calls `Wait(_STK_SEM_TEST_TIMEOUT)`
200 times, incrementing `g_SharedCounter` on each successful acquisition. Task 1 is
also the verifier. Verifies end-to-end gate semantics: no item is consumed before it
is produced.

**Pass condition:** `counter == 200`

---

### Test 8 — `StressTest`
**Tasks:** 0–4 (all 5) — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 400` &nbsp;|&nbsp; **Initial count:** `0`

Task 0 is the sole producer, firing 400 signals with a `Delay(1)` every 10 iterations
to allow consumers to keep up. Tasks 1–4 are consumers, each draining
`iterations / 4 = 100` signals via `Wait(_STK_SEM_TEST_TIMEOUT)`. After all consumers
finish, the verifier (task 4) reads `GetCount()` to capture any signals that were
posted but not yet consumed. The invariant `consumed + remaining == iterations` must
hold exactly, proving no signal was lost or double-counted under contention.

**Pass condition:** `g_SharedCounter + g_TestSemaphore.GetCount() == 400`

---

## Summary Table

| # | Test | Tasks | Initial count | Pass condition | What it verifies |
|---|------|-------|---------------|----------------|------------------|
| 1 | `BasicSignalWaitTask` | 0–4 | `0` | `counter == 400` | Every `Signal()` produces exactly one successful `Wait()`; no signals lost or duplicated |
| 2 | `InitialCountTask` | 0–4 | `4` | `counter == 4`, `acquired == false` | Pre-loaded permits drain on the fast path; subsequent `Wait()` blocks when count reaches zero |
| 3 | `TimeoutWaitTask` | 0–2 | `0` | `counter == 2` | `Wait()` returns `false` within `[45, 60]` ms when no signal arrives; semaphore usable after timeout |
| 4 | `ZeroTimeoutTask` | 0–2 | `0` | `counter == 2` | `Wait(NO_WAIT)` returns `false` on empty and `true` on pre-signaled, both immediately without blocking |
| 5 | `SignalBeforeWaitTask` | 0–2 | `0` | `count_after_signals == 200`, `counter == 200` | Signals posted with no waiters are remembered in the counter; subsequent `Wait()` drains them on the fast path |
| 6 | `FIFOOrderTask` | 0–4 | `0` | order `[1,2,3,4]` | Blocked tasks are woken in FIFO arrival order |
| 7 | `BoundedBufferTask` | 0–2 | `0` | `counter == 200` | Classic single-producer / single-consumer gate: no item consumed before it is produced |
| 8 | `StressTestTask` | 0–4 | `0` | `consumed + remaining == 400` | No signal lost or double-counted under full five-task contention; runs on all platforms including M0 |
