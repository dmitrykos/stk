# SpinLock Test Suite

Test suite for `stk::sync::SpinLock`.  
Source: `test/spinlock/test_spinlock.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_spinlock.h>
```

`SpinLock` is a **recursive** spinlock intended for extremely short critical sections
where the overhead of a kernel context switch (as used by `Mutex`) is undesirable.
Rather than suspending the calling task and entering the kernel wait list, it busy-waits
on a low-level `hw::SpinLock`. To prevent priority livelocks under sustained contention,
once the spin count is exhausted it calls `stk::Yield()` to allow other tasks to run.

Ownership is tracked by task ID (`m_owner_tid`). The recursion count (`m_recursion_count`)
allows the owning task to acquire the lock again without deadlocking; the lock is only
fully released when `Unlock()` has been called enough times to bring the count to zero.

Does **not** require `KERNEL_SYNC`; uses kernel mode `KERNEL_DYNAMIC` only.

```cpp
// Construction
stk::sync::SpinLock g_Lock;         // default spin count = 4000
stk::sync::SpinLock g_Lock(2000);   // custom spin count
stk::sync::SpinLock g_Lock(10);     // very low — forces Yield() quickly (used in tests)
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Lock` | `void Lock()` | Acquires the lock, spinning up to `spin_count` iterations then calling `Yield()` and retrying. Recursive re-entry by the owner increments recursion count and returns immediately. ISR-unsafe. |
| `TryLock` | `bool TryLock()` | Attempts to acquire without spinning. Returns `true` if acquired or already owned (recursive); `false` if held by another task. Never blocks or yields. ISR-unsafe. |
| `Unlock` | `void Unlock()` | Decrements recursion count. When count reaches zero, clears `m_owner_tid` and releases the underlying `hw::SpinLock`. Asserts the caller is the current owner and the lock is held. ISR-unsafe. |

**Lock paths in `Lock()` and `TryLock()`:**

```
caller == owner (recursive)         →  ++m_recursion_count, return true immediately
caller != owner, hw lock free       →  m_owner_tid = caller, m_recursion_count = 1, return true
caller != owner, hw lock contended:
  Lock()    →  spin up to spin_count; call Yield() and retry until acquired
  TryLock() →  return false immediately (no spinning, no yielding)
```

**Release path in `Unlock()`:**

```
--m_recursion_count > 0   →  still held recursively, return (no release)
--m_recursion_count == 0  →  m_owner_tid = 0, release hw::SpinLock
```

**Key invariants:**

- The same task may acquire the lock multiple times; each `Lock()` or successful
  `TryLock()` must be paired with exactly one `Unlock()`.
- `Unlock()` asserts `m_owner_tid == GetTid()` — only the owning task may release.
- `TryLock()` by the owner always returns `true` (recursive path), never competing
  with the hardware lock.
- Maximum recursion depth is `0xFFFE`; exceeding it triggers an assertion.
- Prefer `Mutex` when critical sections involve I/O, blocking calls, or non-trivial
  logic — prolonged spinning wastes CPU cycles.
- ISR-unsafe; use `hw::CriticalSection` for ISR-accessible shared state.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_SL_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_SL_TEST_SPIN_COUNT` | `10` | Spin count for `g_TestSpinLock` — deliberately low to force `Yield()` quickly under contention |
| `_STK_SL_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_SL_TEST_LONG_SLEEP` | `100` ticks | Sleep used by verifier tasks to wait for workers |
| `_STK_SL_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestSpinLock` is a plain static constructed once with `_STK_SL_TEST_SPIN_COUNT = 10`.
Like the Mutex suite, `ResetTestState()` does **not** reconstruct it via placement-new;
it resets only counters and flags. The spin count of 10 ensures that any task failing to
acquire the lock calls `Yield()` within microseconds, making the cooperative-yield path
exercised on every test.

`NeedsExtendedTasks` excludes `TryLockFree`, `TryLockContended`, and `RecursiveTryLock`
from tasks 3–4 (those three tests run tasks 0–2 only). All other tests use all five tasks.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eight
distinct task class templates simultaneously. Tests 1–7 are skipped on M0 and only
`StressTest` (test 8) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory.

`RecursiveLockTask` (test 4) uses a hardcoded stack of `1024` words regardless of
platform, because the `DEPTH = 8` recursive call chain requires more stack than
`_STK_SL_STACK_SIZE` provides.

| Platform | `_STK_SL_STACK_SIZE` | `RecursiveLockTask` stack |
|----------|----------------------|---------------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words | `1024` words |
| All others | `256` words | `1024` words |

---

## Tests

### Test 1 — `MutualExclusion`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks race to increment `g_SharedCounter` inside `Lock()` / `Unlock()` for
100 iterations each. The increment is deliberately non-atomic: each task reads
`temp = g_SharedCounter`, optionally calls `Delay(1)` every 4th iteration to widen
the race window, then writes `g_SharedCounter = temp + 1`. Task 0 uses a
`g_InstancesDone` completion barrier before verifying. If mutual exclusion is broken
any concurrent read-modify-write produces a lower total than expected.

**Pass condition:** `counter == 500` (`5 tasks × 100 iterations`)

---

### Test 2 — `TryLockFree`
**Tasks:** 0–2 only

Verifies `TryLock()` across three states in sequence. Task 1 calls `TryLock()` on a
free lock (must succeed), sets `g_SharedCounter = 2` as a signal to task 2, then
sleeps `_STK_SL_TEST_SHORT_SLEEP` while still holding. Task 2 waits for the signal
then calls `TryLock()` while task 1 holds — must return `false` and increment the
counter to `3`. After task 2 records its result, task 1 calls `Unlock()` then
immediately `TryLock()` again to confirm the lock is free — must return `true` and
increment to `4`. Task 0 is the verifier.

**Pass condition:** `counter == 4`
(2 = task 1 acquired; 3 = task 2 correctly failed while task 1 held; 4 = task 1 re-acquired after release)

---

### Test 3 — `TryLockContended`
**Tasks:** 0–2 only

Task 0 acquires the lock with `Lock()`, sets `g_SharedCounter = 1` to signal task 1,
then sleeps `_STK_SL_TEST_LONG_SLEEP` while holding. Task 1 busy-waits on the counter
then calls `TryLock()`; with the lock held by task 0 it must return `false` immediately
(elapsed < `_STK_SL_TEST_SHORT_SLEEP`) and increment the counter to `2`. Task 2 sleeps
past the hold period then verifies.

**Pass condition:** `counter == 2`
(1 = task 0 signaled it holds the lock; 2 = task 1's `TryLock()` correctly returned false immediately)

---

### Test 4 — `RecursiveLock`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Stack:** `1024` words &nbsp;|&nbsp; **Depth:** `DEPTH = 8`

Each task calls `AcquireRecursive(8)` — a recursive function that calls `Lock()`,
recurses one level deeper, increments `g_SharedCounter` on the way back up, then calls
`Unlock()`. This produces 8 nested acquisitions and 8 matching releases per task.
Verifies that the recursive ownership path (`m_owner_tid == current_tid`) handles
arbitrary depth correctly and that the lock is only fully released by the final
`Unlock()` at depth 0. Task 0 uses a `g_InstancesDone` completion barrier.

**Pass condition:** `counter == 40` (`5 tasks × 8 depth levels`)

---

### Test 5 — `RecursiveTryLock`
**Tasks:** 0–2 only

Verifies `TryLock()` recursive ownership semantics. Task 1 calls `TryLock()` three
times (depths 1, 2, 3) — all must return `true` because the recursive path applies.
Task 1 then calls `Unlock()` twice (returning to depth 1), sets `g_SharedCounter = 2`
as a signal, and sleeps while still holding depth 1. Task 2 waits for the signal and
calls `TryLock()`; with the lock held by task 1 at depth 1 it must return `false` and
increment the counter to `3`. Task 1 then calls its final `Unlock()`. Task 0 is the
verifier.

**Pass condition:** `counter == 3`
(1 = all three recursive `TryLock()` calls succeeded; 2 = task 1 signaled depth 1 still held; 3 = task 2's `TryLock()` correctly failed)

---

### Test 6 — `YieldUnderContention`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 200`

All five tasks hammer the lock simultaneously with no pacing. With `spin_count = 10`,
any task failing to acquire after 10 spins calls `Yield()` and retries from the
scheduler's next opportunity. Verifies that the cooperative-yield mechanism prevents
livelock under maximum contention: every increment must complete and the total must be
exact. Task 0 uses a `g_InstancesDone` completion barrier.

**Pass condition:** `counter == 1000` (`5 tasks × 200 iterations`)

---

### Test 7 — `UnlockTransfer`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 50`

All five tasks compete for the lock equally: acquire, increment, release, `Delay(1)`.
The `Delay(1)` staggers re-acquisitions so that after each release a different task is
likely to win, exercising the transfer from one spinner to the next across all five
tasks. Verifies that no increment is lost or duplicated across the full transfer chain.
Task 0 uses a `g_InstancesDone` completion barrier.

**Pass condition:** `counter == 250` (`5 tasks × 50 iterations`)

---

### Test 8 — `StressTest`
**Tasks:** 0–4 (all 5) — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 400`

All five tasks run 400 iterations alternating between two lock strategies by iteration
index: even iterations (`i % 2 == 0`) use blocking `Lock()` (always succeeds); odd
iterations (`i % 2 == 1`) use non-blocking `TryLock()` (may fail under contention).
Every successful acquisition increments `g_SharedCounter` before releasing. Task 4
uses a `g_InstancesDone` completion barrier. The minimum is computed from the
guaranteed blocking contributions only.

Minimum guaranteed count: `Lock()` iterations only = `5 × (400 / 2)` = `5 × 200 = 1000`.

**Pass condition:** `counter >= 1000`

---

## Summary Table

| # | Test | Tasks | Stack | Pass condition | What it verifies |
|---|------|-------|-------|----------------|------------------|
| 1 | `MutualExclusionTask` | 0–4 | `_STK_SL_STACK_SIZE` | `counter == 500` | `Lock()` / `Unlock()` provides mutual exclusion; no increment lost under deliberate read-modify-Delay-write race |
| 2 | `TryLockFreeTask` | 0–2 | `_STK_SL_STACK_SIZE` | `counter == 4` | `TryLock()` succeeds on a free lock; returns `false` while another task holds; succeeds again after release |
| 3 | `TryLockContendedTask` | 0–2 | `_STK_SL_STACK_SIZE` | `counter == 2` | `TryLock()` returns `false` immediately (no spinning) when the lock is held by another task |
| 4 | `RecursiveLockTask` | 0–4 | `1024` words | `counter == 40` | Recursive `Lock()` to depth 8 does not deadlock; fully released only after the matching `Unlock()` at depth 0 |
| 5 | `RecursiveTryLockTask` | 0–2 | `_STK_SL_STACK_SIZE` | `counter == 3` | `TryLock()` succeeds for the owner at any recursion depth; correctly fails for a non-owner while any depth remains |
| 6 | `YieldUnderContentionTask` | 0–4 | `_STK_SL_STACK_SIZE` | `counter == 1000` | Cooperative `Yield()` triggered after `spin_count = 10` prevents livelock; all increments complete correctly |
| 7 | `UnlockTransferTask` | 0–4 | `_STK_SL_STACK_SIZE` | `counter == 250` | Lock transfers cleanly across all five tasks on each release; no increment lost or duplicated |
| 8 | `StressTestTask` | 0–4 | `_STK_SL_STACK_SIZE` | `counter >= 1000` | No corruption under full five-task contention mixing blocking `Lock()` and non-blocking `TryLock()`; runs on all platforms |
