# Mutex Test Suite

Test suite for `stk::sync::Mutex`.  
Source: `test/mutex/test_mutex.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_mutex.h>
```

`Mutex` is a **recursive** mutex: the owning task may call `Lock()` again without
deadlocking. Each nested acquisition increments an internal recursion counter; the
lock is only fully released when `Unlock()` has been called an equal number of times.

Ownership is tracked by task ID (`m_owner_tid`). On `Unlock()`, if other tasks are
waiting, ownership is transferred directly to the first waiter in FIFO order — the
count is set to `1` and `m_owner_tid` is updated atomically before the waiter is
woken, so the waiter owns the lock before it even resumes execution.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::Mutex g_Mtx;    // unlocked, no owner
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Lock` | `void Lock()` | Acquires the lock, blocking indefinitely. If already owned by the calling task, increments recursion count and returns immediately. ISR-unsafe. |
| `TryLock` | `bool TryLock()` | Acquires the lock without blocking (`timeout == NO_WAIT`). Returns `true` if acquired, `false` if held by another task. Recursive re-entry by the owner always returns `true`. ISR-unsafe. |
| `TimedLock` | `bool TimedLock(Timeout timeout)` | Acquires the lock, blocking up to `timeout` ticks. Returns `true` if acquired, `false` on timeout. `timeout == 0` is equivalent to `TryLock()`. ISR-unsafe. |
| `Unlock` | `void Unlock()` | Decrements recursion count. When count reaches zero, transfers ownership to the first waiter (FIFO) or marks the mutex free. Asserts the caller is the current owner. ISR-unsafe. |
| `~Mutex` | (destructor) | Asserts `m_wait_list.IsEmpty()` in debug builds. Destroying a mutex with active waiters is a logic error. |

**Lock paths in `TimedLock()`:**

```
caller == owner (recursive)  →  ++m_count, return true immediately
m_count == 0   (free)        →  m_count = 1, m_owner_tid = caller, return true immediately
timeout == 0   (try-lock)    →  return false immediately (NO_WAIT fast-fail)
otherwise      (contended)   →  block until Unlock() transfers ownership or timeout
```

**Ownership transfer in `Unlock()`:**

```
--m_count > 0          →  still held recursively, return (no release)
--m_count == 0, waiters present  →  m_count = 1, m_owner_tid = waiter->GetTid(), Wake(waiter)
--m_count == 0, no waiters       →  m_owner_tid = 0 (fully free)
```

**Key invariants:**

- The same task may acquire the lock multiple times without deadlocking; each `Lock()`
  must be paired with exactly one `Unlock()`.
- `Unlock()` asserts ownership — only the task that holds the lock may release it.
- After ownership transfer in `Unlock()`, `m_count == 1` and `m_owner_tid` is already
  set to the woken task's ID before that task resumes.
- Destroying a `Mutex` while tasks are waiting is a logic error; an assertion fires
  in debug builds.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_MUTEX_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_MUTEX_TEST_TIMEOUT` | `1000` ticks | Blocking timeout for `TimedLock()` calls that must succeed |
| `_STK_MUTEX_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_MUTEX_TEST_LONG_SLEEP` | `100` ticks | Sleep used by verifier tasks to wait for workers |
| `_STK_MUTEX_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestMutex` is a plain static — unlike other test suites, `ResetTestState()` does
**not** reconstruct it via placement-new. The mutex is shared across all test runs in
its naturally unlocked state after each test completes. `ResetTestState()` resets only
the counters and flags.

All eight tests add all five tasks (0–4) unconditionally — there is no
`NeedsExtendedTasks` gating in this suite.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eight
distinct task class templates simultaneously. Tests 1–7 are skipped on M0 and only
`StressTest` (test 8) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory.

`RecursiveDepthTask` (test 6) uses a hardcoded stack of `1024` words regardless of
platform, because the `DEPTH = 8` recursive call chain requires more stack than
`_STK_MUTEX_STACK_SIZE` provides.

| Platform | `_STK_MUTEX_STACK_SIZE` | `RecursiveDepthTask` stack |
|----------|-------------------------|----------------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words | `1024` words |
| All others | `256` words | `1024` words |

---

## Tests

### Test 1 — `BasicLockUnlock`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks race to increment `g_SharedCounter` inside a `Lock()` / `Unlock()`
critical section for 100 iterations each. The increment is deliberately non-atomic:
each task reads `temp = g_SharedCounter`, then writes `g_SharedCounter = temp + 1`,
with a `Delay(1)` injected every 4th increment to widen the race window. Task 0
uses a `g_InstancesDone` completion barrier to wait for all five tasks to finish
before verifying the total. If mutual exclusion is broken, any concurrent read-modify-
write will produce a lower total than expected.

**Pass condition:** `counter == 500` (`5 tasks × 100 iterations`)

---

### Test 2 — `RecursiveLock`
**Tasks:** 0–4 (all 5)

Each task acquires the mutex three times in nested scope (recursion depth 3), increments
`g_SharedCounter` in the innermost scope, then releases three times. Verifies that the
recursive re-entry path in `TimedLock()` (`m_owner_tid == current_tid`) increments
`m_count` and returns immediately without blocking, and that full release only occurs
after the matching number of `Unlock()` calls. Task 0 uses a completion barrier.

**Pass condition:** `counter == 5` (each of 5 tasks incremented exactly once)

---

### Test 3 — `TryLock`
**Tasks:** 0–1 active (tasks 2–4 present but idle)

Task 0 acquires the mutex with `Lock()`, sets `g_SharedCounter = 1`, then sleeps
`_STK_MUTEX_TEST_LONG_SLEEP` ticks while holding it. Task 1 sleeps briefly to let
task 0 establish ownership, then calls `TryLock()` and measures elapsed time. Since
the mutex is held by another task, `TryLock()` must return `false` immediately
(elapsed < `_STK_MUTEX_TEST_SHORT_SLEEP`). `g_TestResult` is set directly inside
task 1's branch.

**Pass condition:** `TryLock()` returned `false` and elapsed < `_STK_MUTEX_TEST_SHORT_SLEEP`

---

### Test 4 — `TimedLock`
**Tasks:** 0–2 active (tasks 3–4 present but idle)

Task 0 holds the mutex for 200 ticks. Task 1 sleeps `_STK_MUTEX_TEST_SHORT_SLEEP`
then calls `TimedLock(50)`; with the mutex still held by task 0, it must time out and
return `false` with elapsed in `[45, 60]` ms. Task 2 sleeps until tick 250 (after task
0 releases) then calls `TimedLock(100)`; the mutex is now free so it must succeed and
increment the counter. Task 2 is the verifier and sleeps `_STK_MUTEX_TEST_LONG_SLEEP`
before checking.

**Pass condition:** `counter == 2`
(1 = `TimedLock(50)` timed out correctly; 2 = `TimedLock(100)` succeeded after release)

---

### Test 5 — `FIFOOrder`
**Tasks:** 0–4 (all 5)

Task 0 acquires the mutex and holds it for 50 ticks while tasks 1–4 queue up. Tasks
1–4 stagger their entry into `Lock()` by sleeping `SHORT_SLEEP × m_task_id` ticks,
ensuring they join the wait list in ascending task-id order (1, 2, 3, 4). As each
consumer acquires and immediately releases the lock, it records its `m_task_id` into
`g_AcquisitionOrder[g_OrderIndex++]`. Task 4 uses a `g_InstancesDone` completion
barrier then verifies the array is exactly `[1, 2, 3, 4]`.

**Pass condition:** `g_AcquisitionOrder == [1, 2, 3, 4]`

---

### Test 6 — `RecursiveDepth`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Stack:** `1024` words &nbsp;|&nbsp; **Depth:** `DEPTH = 8`

Each task calls `RecursiveLock(8)` — a recursive function that calls `Lock()`,
recurses one level deeper, increments `g_SharedCounter` on the way back up, then
calls `Unlock()`. This produces 8 nested acquisitions and 8 releases per task.
Verifies that the recursive path handles arbitrary call depth correctly and that
`m_count` tracks each level precisely. Task 0 sleeps `_STK_MUTEX_TEST_LONG_SLEEP`
before verifying.

**Pass condition:** `counter == 40` (`5 tasks × 8 depth levels`)

---

### Test 7 — `InterTaskCoordination`
**Tasks:** 0–4 (all 5)

All five tasks increment `g_SharedCounter` in strict round-robin turn order for 10
rounds. Each task holds the mutex, checks `g_SharedCounter % 5 == m_task_id`, and
if not its turn, releases the mutex, yields with `Delay(1)`, and re-acquires — busy-
waiting under the mutex without a condition variable. This forces tasks to take turns
in exact task-id order (0, 1, 2, 3, 4, 0, 1, …). Task 4 uses a `g_InstancesDone`
completion barrier then verifies.

**Pass condition:** `counter == 50` (`10 rounds × 5 tasks`)

---

### Test 8 — `StressTest`
**Tasks:** 0–4 (all 5) — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 400`

All five tasks run 400 iterations each, cycling through three lock strategies by
iteration index: `i % 3 == 0` uses `Lock()` / `Unlock()` (always succeeds),
`i % 3 == 1` uses `TryLock()` (may fail under contention),
`i % 3 == 2` uses `TimedLock(10)` (may time out under contention).
A `Delay(1)` is inserted every 10 iterations to allow other tasks to run.
Each successful acquisition increments `g_SharedCounter` before releasing.
Task 4 uses a `g_InstancesDone` completion barrier. The pass condition is deliberately
permissive — only `TryLock` and short-timeout `TimedLock` paths can fail, so the
total is bounded below by the guaranteed `Lock()` contributions.

**Pass condition:** `counter > 0`

---

## Summary Table

| # | Test | Tasks | Stack | Pass condition | What it verifies |
|---|------|-------|-------|----------------|------------------|
| 1 | `BasicLockUnlockTask` | 0–4 | `_STK_MUTEX_STACK_SIZE` | `counter == 500` | `Lock()` / `Unlock()` provides mutual exclusion; no increment lost under deliberate race |
| 2 | `RecursiveLockTask` | 0–4 | `_STK_MUTEX_STACK_SIZE` | `counter == 5` | Recursive re-entry (depth 3) returns immediately without blocking; full release after matching `Unlock()` count |
| 3 | `TryLockTask` | 0–1 | `_STK_MUTEX_STACK_SIZE` | `TryLock() == false`, elapsed < `SHORT_SLEEP` | `TryLock()` returns `false` immediately when mutex is held by another task |
| 4 | `TimedLockTask` | 0–2 | `_STK_MUTEX_STACK_SIZE` | `counter == 2` | `TimedLock()` times out in `[45, 60]` ms when contended; succeeds when mutex is free |
| 5 | `FIFOOrderTask` | 0–4 | `_STK_MUTEX_STACK_SIZE` | order `[1,2,3,4]` | Blocked tasks are granted ownership in FIFO arrival order |
| 6 | `RecursiveDepthTask` | 0–4 | `1024` words | `counter == 40` | Recursive locking to depth 8 tracks `m_count` correctly across all levels and all tasks |
| 7 | `InterTaskCoordinationTask` | 0–4 | `_STK_MUTEX_STACK_SIZE` | `counter == 50` | Mutex correctly gates strict round-robin turn-taking across 10 rounds without a condition variable |
| 8 | `StressTestTask` | 0–4 | `_STK_MUTEX_STACK_SIZE` | `counter > 0` | No corruption or deadlock under full five-task contention mixing `Lock()`, `TryLock()`, and `TimedLock(10)`; runs on all platforms |
