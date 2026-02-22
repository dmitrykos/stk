# RWMutex Test Suite

Test suite for `stk::sync::RWMutex`.  
Source: `test/rwmutex/test_rwmutex.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_rwmutex.h>
```

`RWMutex` is a **reader-writer lock** that allows **multiple concurrent readers** or **one exclusive writer**. It is optimized for resources that are read frequently but modified infrequently. The implementation uses a **writer preference policy** to prevent writer starvation: if a writer is waiting, new readers are blocked until all waiting writers have acquired and released the lock.

Internally `RWMutex` owns a `Mutex` (`m_mutex`), two `ConditionVariable` instances (`m_cv_readers`, `m_cv_writers`), and three counters:
- `m_readers` — number of active readers
- `m_writers_waiting` — number of writers blocked waiting to acquire
- `m_writer_active` — boolean; true if a writer currently holds exclusive access

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::RWMutex g_SettingsLock;  // zero readers, zero writers initially
```

### Reader (shared access) methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `ReadLock` | `void ReadLock()` | Acquires shared read lock. Blocks if a writer is active or if writers are waiting (writer preference). ISR-unsafe. |
| `TimedReadLock` | `bool TimedReadLock(Timeout timeout)` | Acquires shared read lock with timeout. Returns `true` if acquired, `false` on timeout. Blocks if a writer is active or waiting. ISR-unsafe. |
| `TryReadLock` | `bool TryReadLock()` | Non-blocking attempt to acquire read lock. Returns `true` if acquired immediately, `false` if a writer is active or waiting. Equivalent to `TimedReadLock(NO_WAIT)`. ISR-unsafe. |
| `ReadUnlock` | `void ReadUnlock()` | Releases shared read lock. Decrements reader count. If this was the last reader, wakes one waiting writer via `m_cv_writers.NotifyOne()`. ISR-unsafe. |

### Writer (exclusive access) methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `Lock` | `void Lock()` | Acquires exclusive write lock. Blocks until all active readers have released and no other writer is active. ISR-unsafe. Implements `IMutex` interface. |
| `TimedLock` | `bool TimedLock(Timeout timeout)` | Acquires exclusive write lock with timeout. Returns `true` if acquired, `false` on timeout. Blocks until readers drain and no writer is active. ISR-unsafe. |
| `TryLock` | `bool TryLock()` | Non-blocking attempt to acquire write lock. Returns `true` if acquired immediately (no readers, no active writer), `false` otherwise. Equivalent to `TimedLock(NO_WAIT)`. ISR-unsafe. |
| `Unlock` | `void Unlock()` | Releases exclusive write lock. Wakes one waiting writer if any exist (`m_cv_writers.NotifyOne()`), otherwise wakes all waiting readers (`m_cv_readers.NotifyAll()`). ISR-unsafe. Implements `IMutex` interface. |

### RAII helper classes

| Class | Usage | Description |
|-------|-------|-------------|
| `ScopedTimedReadMutex` | `ScopedTimedReadMutex guard(rw, timeout)` | RAII wrapper for `TimedReadLock()`. Constructor attempts acquisition; destructor calls `ReadUnlock()` if lock was acquired. Provides `IsLocked()` to check success. |
| `ScopedTimedLock` | `ScopedTimedLock guard(rw, timeout)` | RAII wrapper for `TimedLock()`. Constructor attempts acquisition; destructor calls `Unlock()` if lock was acquired. Provides `IsLocked()` to check success. |

**Key invariants:**

- **Writer preference policy:** If `m_writers_waiting > 0`, new readers block on `m_cv_readers` even if no writer is currently active. This prevents a continuous stream of readers from starving writers.
- **Lock state transitions:**
  - Readers can acquire only when `!m_writer_active && m_writers_waiting == 0`.
  - Writers can acquire only when `!m_writer_active && m_readers == 0`.
- **Wake policy on release:**
  - `ReadUnlock()` wakes one writer if `m_readers` drops to zero.
  - `Unlock()` (writer release) wakes one writer if any are waiting, otherwise wakes **all** readers.

**Blocking behavior:**
- `ReadLock()` / `TimedReadLock()` loop on `m_cv_readers.Wait()` while `m_writer_active || m_writers_waiting > 0`.
- `Lock()` / `TimedLock()` increment `m_writers_waiting`, loop on `m_cv_writers.Wait()` while `m_writer_active || m_readers > 0`, then decrement `m_writers_waiting` and set `m_writer_active = true`.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_RWMUTEX_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_RWMUTEX_TEST_TIMEOUT` | `1000` ticks | Blocking timeout for operations that must succeed |
| `_STK_RWMUTEX_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_RWMUTEX_TEST_LONG_SLEEP` | `100` ticks | Sleep used by verifier tasks to wait for workers |
| `_STK_RWMUTEX_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestRWMutex` is a **plain static** constructed once at program initialization. It is **not** reconstructed via placement-new between tests (unlike `Pipe` or `ConditionVariable`). The lock naturally returns to an unlocked state (zero readers, zero writers) after each test completes.

All tests add all five tasks unconditionally — there is no `NeedsExtendedTasks()` gating.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link ten distinct task class templates simultaneously. Tests 1–9 are skipped on M0 and only `StressTest` (test 10) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`) instantiated for all five task slots, fitting within the available memory.

| Platform | `_STK_RWMUTEX_STACK_SIZE` |
|----------|---------------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `ConcurrentReaders`
**Tasks:** 0–4 &nbsp;|&nbsp; **Param:** none

Tasks 1–4 are readers. Each acquires `ReadLock()`, increments `g_ReaderCount`, sleeps `_STK_RWMUTEX_TEST_SHORT_SLEEP` ticks while holding the lock, then decrements `g_ReaderCount` and releases. `g_MaxConcurrent` tracks the maximum value `g_ReaderCount` reaches. Task 0 is the verifier, waiting for all four readers to finish then checking the maximum. Confirms that multiple readers can hold the lock simultaneously.

**Pass condition:** `g_MaxConcurrent == 4` (all four reader tasks were concurrent)

---

### Test 2 — `WriterExclusivity`
**Tasks:** 0–4 &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks act as writers. Each runs 100 iterations of: acquire exclusive lock via `Lock()`, read-modify-write `g_SharedCounter` with a deliberate race window (delay every 4th operation), then `Unlock()` and `Yield()`. Task 0 is also the verifier. Confirms that the write lock provides true mutual exclusion and no increments are lost.

**Pass condition:** `counter == 500` (`5 tasks × 100 iterations`)

---

### Test 3 — `WriterStarvation`
**Tasks:** 0–4

Tasks 1–3 are readers that continuously acquire/release read locks in a tight loop until `g_TestComplete` is set. Task 4 waits `_STK_RWMUTEX_TEST_SHORT_SLEEP` then attempts to acquire the write lock; with writer preference enforced, task 4 must acquire within a reasonable time (< 200ms) despite the continuous reader flood. Task 0 is the verifier. Confirms that writer preference prevents writer starvation under a heavy read workload.

**Pass condition:** `counter == 1` (writer acquired within 200ms)

---

### Test 4 — `TimedReadLock`
**Tasks:** 0–2

Task 1 holds the write lock for 200 ticks. Task 2 waits until signaled, then calls `TimedReadLock(50)`; this must time out with elapsed time in `[45, 65]` ms. After task 1 releases (task 2 sleeps 250ms total), task 2 calls `TimedReadLock(100)` which succeeds immediately. Task 0 is the verifier. Confirms that `TimedReadLock()` respects the timeout and returns `false` correctly.

**Pass condition:** `counter == 3`  
(1 = task 1 signaled; 2 = task 2 timed out correctly; 3 = task 2 acquired after release)

---

### Test 5 — `TimedWriteLock`
**Tasks:** 0–2

Task 1 holds a read lock for 200 ticks. Task 2 waits until signaled, then calls `TimedLock(50)`; this must time out with elapsed time in `[45, 65]` ms because a reader is active. After task 1 releases (task 2 sleeps 250ms total), task 2 calls `TimedLock(100)` which succeeds immediately. Task 0 is the verifier. Confirms that `TimedLock()` respects the timeout and returns `false` correctly when readers are active.

**Pass condition:** `counter == 3`  
(1 = task 1 signaled; 2 = task 2 timed out correctly; 3 = task 2 acquired after release)

---

### Test 6 — `TryReadWhileWriter`
**Tasks:** 0–2

Task 1 acquires write lock, signals task 2 via `g_SharedCounter = 1`, then holds the lock for `_STK_RWMUTEX_TEST_LONG_SLEEP` (100 ticks). Task 2 waits for the signal, then calls `TryReadLock()` and measures elapsed time; it must return `false` immediately (elapsed < `_STK_RWMUTEX_TEST_SHORT_SLEEP`). After waiting for task 1 to release, task 2 calls `TryReadLock()` again which succeeds. Task 0 is the verifier. Confirms non-blocking `TryReadLock()` semantics: fails instantly when a writer is active, succeeds after release.

**Pass condition:** `counter == 3`  
(1 = writer signaled; 2 = TryReadLock failed immediately; 3 = TryReadLock succeeded after release)

---

### Test 7 — `ReadUnlockWakesWriter`
**Tasks:** 0–3

Tasks 1 and 2 are readers. Each acquires `ReadLock()` and increments `g_ReaderCount`; they both wait until `g_ReaderCount == 2` to ensure both have acquired. Task 3 is a writer that waits until `g_ReaderCount == 2`, then calls `Lock()` and blocks. Tasks 1 and 2 sleep `_STK_RWMUTEX_TEST_SHORT_SLEEP`, then release one by one. When the last reader (`m_readers` drops to zero) calls `ReadUnlock()`, it wakes task 3 via `m_cv_writers.NotifyOne()`. Task 3 measures elapsed time from `Lock()` call to acquisition; it must be < 50ms. The last reader sets `g_SharedCounter = 1` before releasing. Task 0 is the verifier. Confirms that the last reader immediately wakes a waiting writer, not after a full scheduler tick.

**Pass condition:** `counter == 2`  
(1 = last reader signaled; 2 = writer woken within 50ms)

---

### Test 8 — `WriterPriority`
**Tasks:** 0–3

Task 1 is a reader that holds `ReadLock()`, signals via `g_SharedCounter = 1`, then holds for `_STK_RWMUTEX_TEST_LONG_SLEEP` (100 ticks). Task 2 is a writer that waits for the signal, sleeps `_STK_RWMUTEX_TEST_SHORT_SLEEP`, then calls `Lock()` and blocks (this increments `m_writers_waiting`). Task 3 is a second reader that waits for the signal, sleeps `2 × _STK_RWMUTEX_TEST_SHORT_SLEEP`, then attempts `ReadLock()`; with writer preference enforced, task 3 must block behind task 2 even though task 1 is a reader and could theoretically share access. When task 1 releases, task 2 (writer) acquires first, sets `g_WriterCount = 1`, then releases. Task 3 (reader) then acquires and checks `g_WriterCount`; if it's 1, the writer got priority. Task 0 is the verifier. Confirms the writer preference policy: new readers block when writers are waiting.

**Pass condition:** `counter == 2`  
(1 = reader 1 signaled; 2 = writer acquired before reader 3, confirming writer priority)

---

### Test 9 — `ReaderWriterAlternation`
**Tasks:** 0–4

Three distinct phases test alternating access patterns. Tasks 1–3 are reader/verifier hybrids; task 4 is a writer; task 0 is the final verifier.

**Phase 1 (concurrent readers):** Tasks 1–3 each acquire `ReadLock()`, increment `g_ReaderCount`, track `g_MaxConcurrent`, delay 5ms, decrement `g_ReaderCount`, then release.

**Phase 2 (exclusive writer):** Task 4 sleeps `_STK_RWMUTEX_TEST_SHORT_SLEEP`, then acquires `Lock()`, sets `g_SharedCounter = 1` to signal phase 2 complete, then releases.

**Phase 3 (concurrent readers again):** Tasks 1–3 wait for `g_SharedCounter == 1`, then each acquire `ReadLock()` again, increment `g_ReaderCount`, delay 5ms, decrement, and release.

All tasks except task 0 increment `g_InstancesDone` when finished. Task 0 waits for `g_InstancesDone == 4` then verifies `g_MaxConcurrent >= 2` (at least two readers were concurrent in phase 1 or 3). Confirms that readers can share access concurrently in read phases and that the writer gets exclusive access in the write phase.

**Pass condition:** `g_MaxConcurrent >= 2`

---

### Test 10 — `StressTest`
**Tasks:** 0–4 — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks alternate between reader and writer roles by iteration index. Even iterations (`i % 2 == 0`) use `ReadLock()` / `ReadUnlock()` to read `g_SharedCounter` (no modification). Odd iterations (`i % 2 == 1`) use `Lock()` / `Unlock()` to increment `g_SharedCounter`. Every 10th iteration delays 1ms. Task 4 is also the verifier: it waits for `g_InstancesDone == 5`, then verifies the counter equals the expected number of write operations. Expected writes = `5 tasks × (100 iterations / 2) = 250`. Confirms no data corruption under full five-task contention mixing both lock types.

**Pass condition:** `counter == 250` (`5 tasks × 50 write iterations`)

---

## Summary Table

| # | Test | Tasks | Pass condition | What it verifies |
|---|------|-------|----------------|------------------|
| 1 | `ConcurrentReadersTask` | 0–4 | `max_concurrent == 4` | Multiple readers acquire `ReadLock()` simultaneously; shared access works |
| 2 | `WriterExclusivityTask` | 0–4 | `counter == 500` | Exclusive write lock provides mutual exclusion; no lost increments under contention |
| 3 | `WriterStarvationTask` | 0–4 | `counter == 1` | Writer preference: writer acquires within 200ms despite continuous reader flood |
| 4 | `TimedReadLockTask` | 0–2 | `counter == 3` | `TimedReadLock(50)` times out in `[45, 65]`ms when writer active; succeeds after release |
| 5 | `TimedWriteLockTask` | 0–2 | `counter == 3` | `TimedLock(50)` times out in `[45, 65]`ms when reader active; succeeds after release |
| 6 | `TryReadLockWhileWriterTask` | 0–2 | `counter == 3` | `TryReadLock()` fails immediately when writer active; succeeds after release |
| 7 | `ReadUnlockWakesWriterTask` | 0–3 | `counter == 2` | Last reader releasing wakes waiting writer within 50ms (not after full scheduler tick) |
| 8 | `WriterPriorityTask` | 0–3 | `counter == 2` | New readers blocked behind waiting writer (writer preference policy enforcement) |
| 9 | `ReaderWriterAlternationTask` | 0–4 | `max_concurrent >= 2` | Alternating read/write phases; readers share access, writer gets exclusivity |
| 10 | `StressTestTask` | 0–4 | `counter == 250` | No corruption under full contention; readers and writers alternate; runs on all platforms |
