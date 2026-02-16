# ConditionVariable Test Suite

Test suite for `stk::sync::ConditionVariable`.  
Source: `test/condvar/test_condvar.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_cv.h>
#include <sync/stk_sync_mutex.h>
```

`ConditionVariable` follows the **Monitor pattern**: `Wait()` atomically releases the
associated `Mutex` and suspends the calling task. When the task wakes (via signal or
timeout) the `Mutex` is re-acquired before `Wait()` returns. All operations must be
performed with the mutex held by the caller, except `NotifyOne()` and `NotifyAll()`
which are ISR-safe.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction — both primitives are globals, reconstructed per test via placement-new
stk::sync::Mutex             g_Mtx;
stk::sync::ConditionVariable g_Cond;
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Wait` | `bool Wait(IMutex &mutex, Timeout timeout = WAIT_INFINITE)` | Atomically releases `mutex` and blocks the calling task. Re-acquires `mutex` before returning. Returns `true` if signaled, `false` on timeout. Returns `false` immediately without blocking if `timeout == NO_WAIT`. ISR-unsafe unless `timeout == NO_WAIT`. |
| `NotifyOne` | `void NotifyOne()` | Wakes one waiting task in FIFO arrival order. No-op if no tasks are waiting. ISR-safe. |
| `NotifyAll` | `void NotifyAll()` | Wakes all waiting tasks simultaneously. No-op if no tasks are waiting. ISR-safe. |
| `~ConditionVariable` | (destructor) | Asserts `m_wait_list.IsEmpty()` in debug builds. Destroying a CV with active waiters is a logic error. |

**Canonical usage pattern** (spurious-wakeup-safe):

```cpp
// Consumer
g_Mtx.Lock();
while (!condition_met)
    g_Cond.Wait(g_Mtx, WAIT_INFINITE);
// ... consume shared state ...
g_Mtx.Unlock();

// Producer
g_Mtx.Lock();
condition_met = true;
g_Cond.NotifyOne();
g_Mtx.Unlock();
```

**Key invariants:**

- `Wait()` must be called with the mutex **locked** by the calling task.
- The mutex is released **atomically** with task suspension — no window exists where
  the mutex is held and the task is not yet blocked.
- The mutex is always re-acquired before `Wait()` returns, on both the signaled path
  and the timeout path.
- `Wait(mutex, NO_WAIT)` returns `false` immediately; the implementation short-circuits
  before calling `StartWaiting()`.
- Destroying a `ConditionVariable` while tasks are still waiting is a logic error;
  an assertion fires in debug builds.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_CV_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_CV_TEST_TIMEOUT` | `300` ticks | Blocking timeout for `Wait()` calls that must succeed |
| `_STK_CV_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_CV_TEST_LONG_SLEEP` | `100` ticks | Sleep used by the verifier task to wait for workers |
| `_STK_CV_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

Both `g_TestMutex` and `g_TestCond` are **reconstructed in-place** via placement-new
inside `ResetTestState()` before each test, ensuring no wait-list or ownership state
leaks between runs.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link five
distinct task class templates simultaneously. Tests 1–7 are skipped on M0 and only
`StressTest` (test 8) runs. `StressTest` uses a single task class template instantiated
five times, which fits within the available memory.

Stack sizes differ between platforms because the `sync::Mutex` + `ConditionVariable` +
`IKernelService::StartWaiting()` call chain is deeper than a bare `SpinLock` acquire:

| Platform | `_STK_CV_STACK_SIZE` |
|----------|----------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `NotifyOneWakes`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 20`

Task 0 is the producer; tasks 1–4 are consumers. All four consumers block on
`Wait(WAIT_INFINITE)`. The producer calls `NotifyOne()` 20 times, pacing each call
with `Delay(1)` so each notification has time to land on exactly one blocked waiter
before the next fires. Each successful wake increments `g_SharedCounter`.

**Pass condition:** `counter == 20`

---

### Test 2 — `NotifyAllWakes`
**Tasks:** 0–4 (all 5)

All four consumers (tasks 1–4) block on `Wait(_STK_CV_TEST_TIMEOUT)`. After
`_STK_CV_TEST_SHORT_SLEEP` ticks the producer (task 0) calls `NotifyAll()` once.
Every consumer must be released by that single call.

**Pass condition:** `counter == 4`

---

### Test 3 — `TimeoutExpires`
**Tasks:** 0–2 only

Task 1 calls `Wait(50)` with no producer present; the call must return `false` and
the measured elapsed time must fall within `[45, 65]` ms. Task 2 then blocks with
a generous timeout; the producer (task 0) fires `NotifyOne()` at tick 130 to wake it.
Verifies both the timeout return value and that the condition variable remains usable
after a timeout has occurred.

**Pass condition:** `counter == 2`
(1 = timeout fired correctly in bounds; 2 = subsequent `NotifyOne()` still woke task 2)

---

### Test 4 — `MutexReacquired`
**Tasks:** 0–2 only

Directly verifies the Monitor pattern atomicity guarantee on both wakeup paths:

- **Signaled path** (task 1): holds mutex, calls `Wait(TIMEOUT)`; producer (task 0)
  acquires the mutex while task 1 is suspended — proving it was released — increments
  the counter, then calls `NotifyOne()`. Task 1 resumes and calls `Unlock()`; if the
  mutex was not re-acquired the ownership assert would fire.
- **Timeout path** (task 2): calls `Wait(50)` with no notification; on timeout calls
  `Unlock()` — same ownership assert check on the timeout path.

**Pass condition:** `counter == 3`
(1 = mutex was free during `Wait()`; 2 = signaled path re-acquired; 3 = timeout path re-acquired)

---

### Test 5 — `PredicateLoop`
**Tasks:** 0–4 (all 5)

Exercises the canonical spurious-wakeup-safe `while (!cond) Wait()` pattern. Each
consumer (tasks 1–4) loops: `while (g_SharedCounter < m_task_id) Wait()`. The producer
increments `g_SharedCounter` and calls `NotifyOne()` once per consumer, paced with
`Delay(1)`. Consumers that wake and find the predicate unsatisfied re-enter `Wait()`,
correctly tolerating both spurious wakeups and out-of-order delivery.

**Pass condition:** `counter == 4` (all four consumers exited their predicate loops)

---

### Test 6 — `NotifyOneOrder`
**Tasks:** 0–4 (all 5)

Verifies FIFO wake ordering. Tasks 1–4 block in scheduling order (1, 2, 3, 4). The
producer calls `NotifyOne()` four times with `Delay(1)` between calls. Each woken
consumer records its `m_task_id` into `g_AcquisitionOrder[g_OrderIndex++]`. After all
consumers have been woken, the verifier checks that the recorded IDs are strictly
ascending.

**Pass condition:** `order == [1,2,3,4]` and `counter == 4`

---

### Test 7 — `NoWaitTimeout`
**Tasks:** 0–2 only

Verifies the `NO_WAIT` fast-path in `ConditionVariable::Wait()`. Task 1 calls
`Wait(g_TestMutex, NO_WAIT)`; per the implementation this returns `false` immediately
without calling `StartWaiting()`. Elapsed time must be less than
`_STK_CV_TEST_SHORT_SLEEP`. Task 2 then performs a normal blocking `Wait()` which the
producer wakes correctly, confirming the condition variable is fully functional after
a `NO_WAIT` call.

**Pass condition:** `counter == 2`
(1 = `NO_WAIT` returned `false` immediately; 2 = subsequent blocking `Wait()` woken correctly)

---

### Test 8 — `StressTest`
**Tasks:** 0–4 (all 5) — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks alternate producer and consumer roles each iteration. Even iterations
(0, 2, 4, …): acquire mutex, increment `g_SharedCounter`, call `NotifyOne()`, release.
Odd iterations (1, 3, 5, …): acquire mutex, call `Wait(_STK_CV_TEST_SHORT_SLEEP)`,
release. Consumer `Wait()` calls may time out under contention — this is expected and
does not count as a failure. Task 4 uses a `g_InstancesDone` completion barrier to
ensure all five tasks finish before the result is checked.

Minimum guaranteed count: producer iterations only = `5 × ⌈100/2⌉` = `5 × 50 = 250`.

**Pass condition:** `counter >= 250`

---

## Summary Table

| # | Test | Tasks | Pass condition | What it verifies |
|---|------|-------|----------------|------------------|
| 1 | `NotifyOneWakesTask` | 0–4 | `counter == 20` | `NotifyOne()` wakes exactly one waiter per call; no over- or under-delivery |
| 2 | `NotifyAllWakesTask` | 0–4 | `counter == 4` | `NotifyAll()` wakes every blocked task in a single call |
| 3 | `TimeoutExpiresTask` | 0–2 | `counter == 2` | `Wait()` returns `false` within the correct timing window; CV remains usable after a timeout |
| 4 | `MutexReacquiredTask` | 0–2 | `counter == 3` | Mutex released atomically inside `Wait()` and re-acquired before return, on both signaled and timeout paths |
| 5 | `PredicateLoopTask` | 0–4 | `counter == 4` | Spurious-wakeup-safe `while (!cond) Wait()` pattern drives all consumers to completion |
| 6 | `NotifyOneOrderTask` | 0–4 | order `[1,2,3,4]`, `counter == 4` | `NotifyOne()` releases waiters in FIFO arrival order |
| 7 | `NoWaitTimeoutTask` | 0–2 | `counter == 2` | `Wait(NO_WAIT)` returns `false` immediately without blocking; CV functional afterwards |
| 8 | `StressTestTask` | 0–4 | `counter >= 250` | No corruption under full five-task contention with alternating producer/consumer roles; runs on all platforms |
