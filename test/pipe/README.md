# Pipe Test Suite

Test suite for `stk::sync::Pipe`.  
Source: `test/pipe/test_pipe.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_pipe.h>
```

`Pipe<T, N>` is a thread-safe FIFO ring-buffer for inter-task data transfer. It holds
up to `N` elements of type `T` and implements **blocking** semantics on both ends:

- `Write()` blocks if the pipe is full until a consumer frees a slot or the timeout
  expires.
- `Read()` blocks if the pipe is empty until a producer writes data or the timeout
  expires.

Internally `Pipe` owns two `ConditionVariable` instances (`m_cv_empty`, `m_cv_full`)
plus the `Mutex` embedded in each `ConditionVariable`. It does not expose those
primitives directly; all synchronization is encapsulated.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::Pipe<int32_t, 8>  g_Pipe;           // capacity 8 int32_t elements
stk::sync::Pipe<MyStruct, 4> g_StructPipe;     // capacity 4 struct elements
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Write` | `bool Write(const T &data, Timeout timeout = WAIT_INFINITE)` | Copies one element into the FIFO. Blocks if full until space is available or timeout expires. Returns `true` if written, `false` on timeout. ISR-unsafe. |
| `WriteBulk` | `size_t WriteBulk(const T *src, size_t count, Timeout timeout = WAIT_INFINITE)` | Copies `count` elements from `src`. Blocks until all elements are written or timeout occurs. Returns number of elements actually written (equals `count` unless timeout occurred). ISR-unsafe. |
| `Read` | `bool Read(T &data, Timeout timeout = WAIT_INFINITE)` | Removes one element from the FIFO into `data`. Blocks if empty until data is available or timeout expires. Returns `true` if read, `false` on timeout. ISR-unsafe. |
| `ReadBulk` | `size_t ReadBulk(T *dst, size_t count, Timeout timeout = WAIT_INFINITE)` | Removes `count` elements into `dst`. Blocks until all elements are read or timeout occurs. Returns number of elements actually read (equals `count` unless timeout occurred). ISR-unsafe. |
| `GetSize` | `size_t GetSize() const` | Returns the current number of elements in the pipe. Point-in-time snapshot — may change immediately if another task is active. |
| `IsEmpty` | `bool IsEmpty() const` | Returns `true` if the pipe currently holds no elements. Point-in-time snapshot. |

**Copy strategy in `WriteBulk()` / `ReadBulk()`:**

For non-scalar element types or pipe capacities `< 8`, elements are copied one at a
time with a loop to correctly invoke copy constructors/assignment. For scalar types
with capacity `≥ 8`, split `memcpy` is used for the contiguous portions of the ring
buffer to maximize throughput.

**Key invariants:**

- `Write()` / `Read()` notifications use `NotifyOne()`; `WriteBulk()` / `ReadBulk()`
  use `NotifyAll()` after each batch to wake all waiting tasks immediately.
- `GetSize()` and `IsEmpty()` are unguarded reads of `m_count` — safe to call from
  any task as a non-binding hint, but must not be relied upon for flow control outside
  a critical section.
- Destroying a `Pipe` with tasks still blocked inside `Write()` or `Read()` is a
  logic error; the embedded `ConditionVariable` destructor asserts in debug builds.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_PIPE_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_PIPE_TEST_TIMEOUT` | `300` ticks | Blocking timeout for `Write()` / `Read()` calls that must succeed |
| `_STK_PIPE_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_PIPE_TEST_LONG_SLEEP` | `100` ticks | Sleep used by verifier tasks to wait for workers |
| `_STK_PIPE_CAPACITY` | `8` | Pipe capacity (elements) used by all tests |
| `_STK_PIPE_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestPipe` is **reconstructed in-place** via placement-new inside `ResetTestState()`
before each test. This resets `m_head`, `m_tail`, `m_count`, and both internal
`ConditionVariable` instances to a fully clean state without requiring a separately
managed mutex or condition variable.

`NeedsExtendedTasks` excludes tests 1–6 (all tasks-0-to-2-only tests) from tasks 3–4.
Tests 7 and 8 always use all five tasks.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eight
distinct task class templates simultaneously. Tests 1–7 are skipped on M0 and only
`StressTest` (test 8) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory.

| Platform | `_STK_PIPE_STACK_SIZE` |
|----------|-----------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `BasicWriteRead`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Param:** `iterations = 20`

Task 0 (producer) writes values `0..19` sequentially via `Write(_STK_PIPE_TEST_TIMEOUT)`.
Task 1 (consumer) reads 20 values and checks each against its expected sequence number.
Each matching read increments `g_SharedCounter`. Task 0 sleeps `_STK_PIPE_TEST_LONG_SLEEP`
then verifies. Confirms single-element transfers are correct, complete, and delivered in
FIFO order.

**Pass condition:** `counter == 20`

---

### Test 2 — `WriteBlocksWhenFull`
**Tasks:** 0–2 only

Task 0 writes `_STK_PIPE_CAPACITY` (`8`) values into the pipe without pacing — all
succeed immediately on the fast path while the pipe is empty, incrementing the counter to
`8`. It then issues one more `Write()` which must block because the pipe is now full.
Task 1 sleeps `_STK_PIPE_TEST_SHORT_SLEEP` to let the fill complete, then calls
`Read()` to free one slot. The blocked `Write()` in task 0 unblocks and returns `true`,
incrementing the counter to `9`. Verifies back-pressure and the producer wake-on-space
path.

**Pass condition:** `counter == 9` (`_STK_PIPE_CAPACITY + 1`)

---

### Test 3 — `ReadBlocksWhenEmpty`
**Tasks:** 0–2 only

Task 1 (consumer) calls `Read(_STK_PIPE_TEST_TIMEOUT)` immediately on a freshly reset,
empty pipe — it must block. Task 0 (producer) sleeps `_STK_PIPE_TEST_SHORT_SLEEP` to
ensure the consumer is suspended, then writes the sentinel value `42`. The blocked
`Read()` unblocks, and task 1 checks the received value equals `42` before incrementing
the counter. Verifies consumer suspension and the consumer wake-on-data path.

**Pass condition:** `counter == 1` (received value `42` correctly)

---

### Test 4 — `Timeout`
**Tasks:** 0–2 only (verifier waits on `g_InstancesDone < 3`)

Exercises both timeout paths independently in the same run. Task 1 calls `Read(50)` on
an empty pipe immediately; with no producer it must return `false` with elapsed time in
`[45, 65]` ms. Task 2 sleeps `_STK_PIPE_TEST_LONG_SLEEP` to let task 1's timeout
expire cleanly before it fills the pipe to capacity, then calls `Write(99, 50)` on a
full pipe; with no consumer it must also return `false` within `[45, 65]` ms. Task 0
is the verifier.

**Pass condition:** `counter == 2`
(1 = `Read()` timeout fired within bounds on empty pipe; 2 = `Write()` timeout fired within bounds on full pipe)

---

### Test 5 — `BulkWriteRead`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Param:** `_STK_PIPE_CAPACITY` (`8`)

Task 0 (producer) builds a sequential array `{0, 1, …, 7}` and writes it in one call
via `WriteBulk(src, 8, _STK_PIPE_TEST_TIMEOUT)`. Task 1 (consumer) calls
`ReadBulk(dst, 8, _STK_PIPE_TEST_TIMEOUT)` and verifies the returned count equals `8`
and every element matches its expected sequence number. On full match, `g_SharedCounter`
is set to `8`. Task 0 verifies both `written == 8` and `counter == 8`. Confirms block
transfer correctness and FIFO ordering through the bulk path.

**Pass condition:** `written == 8` and `counter == 8`

---

### Test 6 — `GetSizeIsEmpty`
**Tasks:** 0–2 only (task 1 is the sole active worker)

Task 1 performs a complete fill-and-drain cycle with a single reader/writer so no
concurrent changes can interfere. It first asserts `IsEmpty() == true` and
`GetSize() == 0`. It then writes elements `0..7` one by one, asserting `GetSize() == i + 1`
after each `Write()`. It then reads them back one by one, asserting
`GetSize() == CAPACITY - i - 1` after each `Read()`. Finally it asserts `IsEmpty() == true`
again. Any mismatch sets `all_ok = false`. Task 0 sleeps `_STK_PIPE_TEST_LONG_SLEEP`
then verifies. Confirms that `m_count` is maintained exactly throughout the entire fill
and drain sequence.

**Pass condition:** `counter == 1` (all size assertions passed)

---

### Test 7 — `MultiProducerConsumer`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Param:** `iterations = 20`

Tasks 1 and 2 are producers, each writing `20` values via `Write(_STK_PIPE_TEST_TIMEOUT)`.
Tasks 3 and 4 are consumers, each reading `20` values via `Read(_STK_PIPE_TEST_TIMEOUT)`
and incrementing `g_SharedCounter` on each successful read. Task 0 is the verifier,
waiting on a `g_InstancesDone` completion barrier for all four workers. Total items
written = `2 × 20 = 40`; total items that must be read = `40`. Verifies that the pipe
correctly serialises concurrent access from two simultaneous producers and two simultaneous
consumers without losing or duplicating any element.

**Pass condition:** `counter == 40` (`2 producers × 20 iterations`)

---

### Test 8 — `StressTest`
**Tasks:** 0–4 (all 5) — **runs on all platforms including Cortex-M0** &nbsp;|&nbsp; **Param:** `iterations = 100`

All five tasks alternate producer and consumer roles by iteration index: even iterations
call `Write(i, _STK_PIPE_TEST_SHORT_SLEEP)` (may block briefly if pipe is full); odd
iterations call `Read(_STK_PIPE_TEST_SHORT_SLEEP)` (may time out if pipe is empty).
Each task tracks its own `written` and `consumed` counts and adds `written - consumed`
into `g_SharedCounter`. After all tasks finish, task 4 reads `g_TestPipe.GetSize()` for
any elements still in the pipe. The invariant `g_SharedCounter + remaining >= 0` confirms
that total reads never exceeded total writes — which would indicate data corruption or
a double-read.

**Pass condition:** `g_SharedCounter + g_TestPipe.GetSize() >= 0`

---

## Summary Table

| # | Test | Tasks | Pass condition | What it verifies |
|---|------|-------|----------------|------------------|
| 1 | `BasicWriteReadTask` | 0–2 | `counter == 20` | Single-element `Write()` / `Read()` transfers values correctly in FIFO order |
| 2 | `WriteBlocksWhenFullTask` | 0–2 | `counter == 9` | `Write()` blocks when pipe is at capacity; unblocks atomically when consumer frees a slot |
| 3 | `ReadBlocksWhenEmptyTask` | 0–2 | `counter == 1` | `Read()` blocks on an empty pipe; unblocks when producer writes; received value matches |
| 4 | `TimeoutTask` | 0–2 | `counter == 2` | `Read()` on empty and `Write()` on full both return `false` within `[45, 65]` ms |
| 5 | `BulkWriteReadTask` | 0–2 | `written == 8`, `counter == 8` | `WriteBulk()` / `ReadBulk()` transfers a full block; count and all element values correct |
| 6 | `GetSizeIsEmptyTask` | 0–2 | `counter == 1` | `GetSize()` tracks exactly after every `Write()` and `Read()`; `IsEmpty()` correct before and after |
| 7 | `MultiProducerConsumerTask` | 0–4 | `counter == 40` | Two concurrent producers and two concurrent consumers transfer all items without loss or duplication |
| 8 | `StressTestTask` | 0–4 | `net + remaining >= 0` | No data corruption under full five-task contention mixing blocking writes and reads; runs on all platforms |
