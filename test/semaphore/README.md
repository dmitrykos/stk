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
stk::sync::Semaphore g_Sem(0, 1);  // initial count = 0, max count = 1 (binary semaphore)
```

The counter is bounded by `max_count` (default `sync::Semaphore::COUNT_MAX`).

| Method | Signature | Description |
|--------|-----------|-------------|
| `Wait` | `bool Wait(Timeout timeout = WAIT_INFINITE)` | Decrements counter if `> 0` (fast path, no blocking). If counter is `0` and `timeout == 0` (`NO_WAIT`), returns `false` immediately. Otherwise blocks until `Signal()` or timeout. Returns `true` if acquired, `false` on timeout **or cancellation** (use `WaitEx()` to tell them apart). ISR-unsafe. |
| `WaitEx` | `EWaitResult WaitEx(Timeout timeout = WAIT_INFINITE)` | Same as `Wait()` but reports the full outcome: `WAIT_RESULT_SIGNAL` (acquired, including the fast path), `WAIT_RESULT_TIMEOUT` (timeout expired, or `NO_WAIT` on an empty semaphore), `WAIT_RESULT_CANCELED` (wait cancelled via `IKernel::CancelTaskWait()` / `stk::SleepCancel()`), `WAIT_RESULT_FAIL` (kernel rejected the wait). ISR-unsafe. |
| `Signal` | `void Signal()` | If tasks are waiting, delivers token directly to the first waiter (Direct Handover — counter is not incremented). If no tasks are waiting, increments `m_count`; asserts if the counter is already at `max_count`. ISR-safe. |
| `TrySignal` | `bool TrySignal()` | Like `Signal()`, but returns `false` (counter unchanged) instead of asserting when the counter is at `max_count` and no task is waiting. With a waiter present it still succeeds by direct hand-off, even at `max_count`. ISR-safe. |
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
- A cancelled wait returns `WAIT_RESULT_CANCELED` (`Wait()` returns `false`) immediately,
  regardless of the deadline. The cancelled task is removed from the wait list and receives no
  token, so the counter is unchanged and the next `Signal()` is delivered to the next waiter or
  remembered in the counter, never swallowed by the cancelled task.
- If a `Signal()` hand-over already woke the waiter before the cancel arrives, the cancel is a
  no-op: the waiter owns the token and gets `WAIT_RESULT_SIGNAL`; the token is neither lost nor
  duplicated.
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
`ResetTestState()` before each test. `ResetTestState()` accepts `initial_count` and
`max_count` parameters forwarded to the `Semaphore` constructor, allowing each test to
start with the exact counter value and bound it requires.

Cancellation tests (10–21) use additional shared state, all cleared by `ResetTestState()`:

| Variable | Purpose |
|----------|---------|
| `g_Victim` / `g_VictimTid` | Task (as `ITask *` and as `TId`) whose wait is cancelled; published by task 1 just before it blocks |
| `g_VictimResult[2]` | `EWaitResult` of the wait of task 1 / task 2 (`-100` = not set); for `CancelWait_BoolWait` task 1 stores `1` / `0` = `Wait()` returned `true` / `false` |
| `g_VictimElapsed[2]` | Milliseconds spent in the wait by task 1 / task 2 |
| `g_VictimSleepElapsed` | Milliseconds task 1 spent in `Sleep(60)` after its wait ended (`NoStaleWaiter` only) |
| `g_CancelParam` | Scenario selector, set by `main()` before each `CancelWait_*` `RunTest()` call |

`CancelVictim(use_sleep_cancel)` cancels the victim's wait either via
`g_Kernel.CancelTaskWait(g_Victim)` or via `stk::SleepCancel(g_VictimTid)`. The scenario is
selected by the global `g_CancelParam`; adding `CW_VIA_SLEEPCANCEL` (`100`) switches the cancel
path to `stk::SleepCancel()`.

The scenario is a global rather than a constructor argument on purpose: `RunTest()` declares tasks
0–2 as `static` on non-M0 platforms, and a static local is constructed only once per template
instantiation, so a constructor argument would silently keep the value of the first `CancelWait_*`
case for every later case. `CancelWaitTask` therefore reads `g_CancelParam` at the start of `Run()`.

Tasks 0–2 are always added by `RunTest()`; tasks 3–4 are added unless the test is `TimeoutWait`,
`ZeroTimeout`, `SignalBeforeWait`, `BoundedBuffer`, `TrySignalSaturation`, `AlreadySignaledOnInit`
or a `CancelWait_*` test.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link all the
distinct task class templates simultaneously. Tests 1–21 are skipped on M0 and only
`StressTest` (test 22) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory. Tests 1–21
introduce distinct task class templates (tests 10–21 share one, `CancelWaitTask`), multiplying the static memory footprint
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

### Test 8 — `TrySignalSaturation`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Initial count:** `0` &nbsp;|&nbsp; **Max count:** `1`

Verifies `TrySignal()` at the `max_count` boundary. Task 0 calls `TrySignal()` (0 → 1, succeeds), then
`TrySignal()` again: the counter is saturated and nobody waits, so it must return `false` and leave the
counter at 1. It drains the permit with `Wait(NO_WAIT)`, sleeps so that task 1 blocks in `Wait()`, then
calls `TrySignal()` once more: with a waiter present it must succeed by direct hand-off, without
incrementing the counter. Task 1 records that it acquired the permit.

**Pass condition:** first `TrySignal()` true, second false, `count == 1`, drain true, hand-off `TrySignal()`
true, `count == 0` after hand-off, and task 1 acquired the permit (`counter == 1`)

---

### Test 9 — `AlreadySignaledOnInit`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Initial count:** `1` &nbsp;|&nbsp; **Max count:** `1`

Verifies a semaphore constructed saturated (`Semaphore(1, 1)`) without any `Signal()` call. Task 0 checks that
`GetCount()` is already 1, that `TrySignal()` reports failure (saturated, no waiters) and leaves the counter
unchanged, that `Wait(NO_WAIT)` drains the pre-loaded permit (true), and that a second `Wait(NO_WAIT)` fails.

**Pass condition:** `count_at_init == 1`, `TrySignal()` false, count still 1, first `Wait(NO_WAIT)` true,
second false

---

### Tests 10–21 — `CancelWait_*`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Task class:** `CancelWaitTask` &nbsp;|&nbsp; **Initial count:** `0`

Verify that a task blocked on the semaphore can be cancelled. Task 0 is the canceller and checker, task 1 is
the victim blocked in `WaitEx()` from ~10 ms, and task 2 is a second waiter used only by the two-waiters
scenario. Task 0 cancels at ~60 ms. Because the semaphore starts at 0, a later `Signal()` either hands its
token to a waiter (counter stays 0, Direct Handover) or is remembered (counter + 1): which of the two
happens shows whether a cancelled task was correctly removed from the wait list. All scenarios run through
`IKernel::CancelTaskWait()`; the `SleepCancel` variants (tests 17–21) repeat a subset through
`stk::SleepCancel()`.

If a cancel fails, task 0 signals the semaphore three times so any still-blocked waiter is released and the
test fails instead of hanging the kernel.

#### Test 10 — `CancelWait_Infinite`
Task 1 calls `WaitEx(WAIT_INFINITE)` and is cancelled by task 0.

**Pass condition:** result is `WAIT_RESULT_CANCELED`, elapsed time is in `[35, 75]` ms, and `GetCount() == 0`
(the cancel does not create a permit).

#### Test 11 — `CancelWait_NotTimeout`
Task 1 waits with a 300 ms timeout and is cancelled long before it expires.

**Pass condition:** result is `WAIT_RESULT_CANCELED` (not `WAIT_RESULT_TIMEOUT`), elapsed time is below
150 ms, and `GetCount() == 0`.

#### Test 12 — `CancelWait_NoStaleWaiter`
After the cancel, task 0 calls `Signal()` twice with nobody waiting. A stale wait entry would swallow the
first token (hand it to a task that is not waiting). Task 1 meanwhile sleeps 60 ms; a ghost wait entry would
wake it early.

**Pass condition:** result is `WAIT_RESULT_CANCELED`; `GetCount() == 2`; two `Wait(NO_WAIT)` calls succeed and
a third fails; task 1 slept at least 55 ms.

#### Test 13 — `CancelWait_TwoWaiters`
Tasks 1 and 2 both block on the semaphore; only task 1 is cancelled. Task 0 then calls `Signal()`.

**Pass condition:** task 1 gets `WAIT_RESULT_CANCELED`; task 2 gets `WAIT_RESULT_SIGNAL`; `GetCount() == 0`
(the token was handed over, not remembered); a following `Wait(NO_WAIT)` fails (nothing left over).

#### Test 14 — `CancelWait_SignalWins`
Task 0 calls `Signal()` and `CancelTaskWait()` back to back with no scheduling point between them. The waiter
already holds the token when the cancel arrives, so the cancel must be a no-op.

**Pass condition:** task 1 gets `WAIT_RESULT_SIGNAL`; `GetCount() == 0`; a following `Wait(NO_WAIT)` fails
(the token is neither lost nor duplicated).

#### Test 15 — `CancelWait_BoolWait`
Task 1 uses the boolean `Wait(WAIT_INFINITE)` instead of `WaitEx()`. A cancel is reported as `false`; since an
infinite wait can not time out, `false` after ~50 ms identifies the cancel. Task 0 then signals once with nobody
waiting.

**Pass condition:** `Wait()` returned `false` with elapsed time in `[35, 75]` ms; `GetCount() == 1` after the
`Signal()`; `Wait(NO_WAIT)` succeeds.

#### Test 16 — `CancelWait_RetryAfter`
After receiving `WAIT_RESULT_CANCELED`, task 1 immediately calls `WaitEx(WAIT_INFINITE)` again on the same
semaphore. Verifies that a cancelled task can re-enter the wait list cleanly.

**Pass condition:** first wait is `WAIT_RESULT_CANCELED`; task 1 is blocked again before the `Signal()`; its
second wait returns `WAIT_RESULT_SIGNAL`; `GetCount() == 0`.

#### Tests 17–21 — `CancelWait_SleepCancel*`
Same scenarios as tests 10, 12, 13, 14 and 15, but cancelled through `stk::SleepCancel(tid)`:

| # | Test | Repeats |
|---|------|---------|
| 17 | `CancelWait_SleepCancel` | Test 10 (`Infinite`) |
| 18 | `CancelWait_SleepCancelNoStale` | Test 12 (`NoStaleWaiter`) |
| 19 | `CancelWait_SleepCancelTwo` | Test 13 (`TwoWaiters`) |
| 20 | `CancelWait_SleepCancelSignal` | Test 14 (`SignalWins`) |
| 21 | `CancelWait_SleepCancelBool` | Test 15 (`BoolWait`) |

**Pass condition:** same as the corresponding test above.

---

### Test 22 — `StressTest`
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
| 8 | `TrySignalSaturationTask` | 0–2 | `0` (max `1`) | `TrySignal()` true, false, hand-off true; counts 1 / 0; waiter acquired | `TrySignal()` rejects at `max_count` without a waiter and hands off to a waiter even at `max_count` |
| 9 | `AlreadySignaledOnInitTask` | 0–2 | `1` (max `1`) | `count == 1`, `TrySignal()` false, first `Wait(NO_WAIT)` true, second false | A semaphore constructed saturated reports its count, rejects `TrySignal()` and drains normally |
| 10 | `CancelWait_Infinite` | 0–2 | `0` | `WAIT_RESULT_CANCELED`, elapsed `[35, 75]` ms, `count == 0` | Cancel ends an infinite wait with `WAIT_RESULT_CANCELED` and creates no permit |
| 11 | `CancelWait_NotTimeout` | 0–2 | `0` | `WAIT_RESULT_CANCELED`, elapsed `< 150` ms | A cancel is not reported as a timeout and ends a timed wait early |
| 12 | `CancelWait_NoStaleWaiter` | 0–2 | `0` | `count == 2` after two `Signal()`, victim sleep `>= 55` ms | Cancel leaves no wait entry that would swallow a token or wake the task early |
| 13 | `CancelWait_TwoWaiters` | 0–2 | `0` | first `CANCELED`, second `SIGNAL`, `count == 0`, no leftover | Cancelling one waiter leaves the other intact; the next token is handed to it |
| 14 | `CancelWait_SignalWins` | 0–2 | `0` | victim `SIGNAL`, `count == 0`, no leftover | A `Signal()` hand-over that arrived first wins over a late cancel; token neither lost nor duplicated |
| 15 | `CancelWait_BoolWait` | 0–2 | `0` | `Wait()` false after `[35, 75]` ms, `count == 1` after `Signal()` | Boolean `Wait()` collapses a cancel into `false`; no stale waiter afterwards |
| 16 | `CancelWait_RetryAfter` | 0–2 | `0` | first `CANCELED`, second `SIGNAL`, `count == 0` | A cancelled task can wait on the semaphore again and receives the next token |
| 17 | `CancelWait_SleepCancel` | 0–2 | `0` | as test 10 | Test 10 via `stk::SleepCancel()` |
| 18 | `CancelWait_SleepCancelNoStale` | 0–2 | `0` | as test 12 | Test 12 via `stk::SleepCancel()` |
| 19 | `CancelWait_SleepCancelTwo` | 0–2 | `0` | as test 13 | Test 13 via `stk::SleepCancel()` |
| 20 | `CancelWait_SleepCancelSignal` | 0–2 | `0` | as test 14 | Test 14 via `stk::SleepCancel()` |
| 21 | `CancelWait_SleepCancelBool` | 0–2 | `0` | as test 15 | Test 15 via `stk::SleepCancel()` |
| 22 | `StressTestTask` | 0–4 | `0` | `consumed + remaining == 400` | No signal lost or double-counted under full five-task contention; runs on all platforms including M0 |
