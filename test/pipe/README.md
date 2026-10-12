# Pipe Test Suite

Test suite for `stk::sync::Pipe` and `stk::sync::PipeT`.  
Source: `test/pipe/test_pipe.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_pipe.h>
```

### `stk::sync::PipeT<T, N>`

Type-safe, compile-time-sized FIFO ring-buffer with internal storage. Holds up to `N`
elements of type `T`. Direct typed assignment — no `memcpy` overhead for scalar types.

```cpp
stk::sync::PipeT<int32_t, 8>  g_Pipe;       // capacity 8 int32_t elements
stk::sync::PipeT<MyStruct, 4> g_StructPipe; // capacity 4 struct elements
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Write` | `bool Write(const T &data, Timeout timeout = WAIT_INFINITE)` | Copies one element into the FIFO. Blocks if full. Returns `true` if written, `false` on timeout. ISR-unsafe. |
| `TryWrite` | `bool TryWrite(const T &data)` | Non-blocking `Write`. Returns `false` instantly if full. ISR-safe. |
| `WriteBulk` | `size_t WriteBulk(const T *src, size_t count, Timeout timeout = WAIT_INFINITE)` | Copies `count` elements. Blocks until all written or timeout. Returns elements written. ISR-unsafe. |
| `Read` | `bool Read(T &data, Timeout timeout = WAIT_INFINITE)` | Removes one element. Blocks if empty. Returns `true` if read, `false` on timeout. ISR-unsafe. |
| `TryRead` | `bool TryRead(T &data)` | Non-blocking `Read`. Returns `false` instantly if empty. ISR-safe. |
| `ReadBulk` | `size_t ReadBulk(T *dst, size_t count, Timeout timeout = WAIT_INFINITE)` | Removes `count` elements. Blocks until all read or timeout. Returns elements read. ISR-unsafe. |
| `GetCount` | `size_t GetCount() const` | Current number of elements. Point-in-time snapshot. |
| `IsEmpty` | `bool IsEmpty() const` | `true` if pipe holds no elements. Point-in-time snapshot. |
| `IsFull` | `bool IsFull() const` | `true` if pipe is at capacity. Point-in-time snapshot. |

### `stk::sync::Pipe`

Runtime-sized, untyped FIFO ring-buffer over an **external** backing buffer.
Parameterised on `element_size` (bytes) rather than a C++ type; all transfers use
`memcpy`. Suitable for C-ABI structs and heterogeneous payloads. Exposes the full
`ReadBulkTriggered` API used by the FreeRTOS stream-buffer wrapper.

```cpp
static uint8_t s_buf[8 * sizeof(MyStruct)];
stk::sync::Pipe g_Pipe(s_buf, 8, sizeof(MyStruct)); // capacity 8, element 4 bytes
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Write` | `bool Write(const void *data, Timeout timeout = WAIT_INFINITE)` | Copies one element (`element_size` bytes). Blocks if full. ISR-unsafe. |
| `TryWrite` | `bool TryWrite(const void *data)` | Non-blocking `Write`. ISR-safe. |
| `WriteBulk` | `size_t WriteBulk(const void *src, size_t count, Timeout timeout = WAIT_INFINITE)` | Copies `count` elements. Blocks until all written or timeout. ISR-unsafe. |
| `TryWriteBulk` | `size_t TryWriteBulk(const void *src, size_t count)` | Non-blocking `WriteBulk`. ISR-safe. |
| `Read` | `bool Read(void *data, Timeout timeout = WAIT_INFINITE)` | Removes one element. Blocks if empty. ISR-unsafe. |
| `TryRead` | `bool TryRead(void *data)` | Non-blocking `Read`. ISR-safe. |
| `ReadBulk` | `size_t ReadBulk(void *dst, size_t count, Timeout timeout = WAIT_INFINITE)` | Removes `count` elements. Blocks until all read or timeout. ISR-unsafe. |
| `TryReadBulk` | `size_t TryReadBulk(void *dst, size_t count)` | Non-blocking `ReadBulk`. ISR-safe. |
| `ReadBulkTriggered` | `size_t ReadBulkTriggered(void *dst, size_t trigger, size_t max_count, Timeout timeout = WAIT_INFINITE)` | Blocks until at least `trigger` elements are present, then drains up to `max_count` in one atomic critical-section pass. With `NO_WAIT` the trigger is ignored and whatever is available is returned immediately. Returns elements read; `< trigger` means timeout fired before threshold was reached. ISR-unsafe (ISR-safe with `NO_WAIT`). |
| `TryReadBulkTriggered` | `size_t TryReadBulkTriggered(void *dst, size_t max_count)` | Non-blocking `ReadBulkTriggered` (trigger = 1, `NO_WAIT`). ISR-safe. |
| `Reset` | `void Reset()` | Discards all elements, resets head/tail/count to zero, wakes blocked producers. Does **not** unblock blocked readers — matches FreeRTOS `xStreamBufferReset` semantics. ISR-safe. |
| `GetCapacity` | `size_t GetCapacity() const` | Construction-time capacity (elements). |
| `GetElementSize` | `size_t GetElementSize() const` | Construction-time element size (bytes). |
| `GetCount` | `size_t GetCount() const` | Current element count. Point-in-time snapshot. |
| `GetSpace` | `size_t GetSpace() const` | Current free slots. Point-in-time snapshot. |
| `IsEmpty` | `bool IsEmpty() const` | `true` if pipe holds no elements. |
| `IsFull` | `bool IsFull() const` | `true` if pipe is at capacity. |

**`*Ex` variants (cancelable waits):** the blocking calls have `EWaitResult`-returning
counterparts that let the caller tell a timeout from a cancellation. The `bool` / `size_t`
variants above collapse both outcomes into "not completed" (`false` / short count).

| Method | Signature | Returns |
|--------|-----------|---------|
| `WriteEx` | `EWaitResult WriteEx(const void *data, Timeout timeout = WAIT_INFINITE)` | `WAIT_RESULT_SIGNAL` if written, `WAIT_RESULT_TIMEOUT` / `WAIT_RESULT_CANCELED` otherwise |
| `ReadEx` | `EWaitResult ReadEx(void *data, Timeout timeout = WAIT_INFINITE)` | `WAIT_RESULT_SIGNAL` if read, `WAIT_RESULT_TIMEOUT` / `WAIT_RESULT_CANCELED` otherwise |
| `WriteBulkEx` | `EWaitResult WriteBulkEx(const void *src, size_t count, size_t &written, Timeout timeout = WAIT_INFINITE)` | Result of the wait; `written` holds the elements stored |
| `ReadBulkEx` | `EWaitResult ReadBulkEx(void *dst, size_t count, size_t &read_count, Timeout timeout = WAIT_INFINITE)` | Result of the wait; `read_count` holds the elements removed |
| `ReadBulkTriggeredEx` | `EWaitResult ReadBulkTriggeredEx(void *dst, size_t trigger, size_t max_count, size_t &read_count, Timeout timeout = WAIT_INFINITE)` | Result of the wait; `read_count` holds the elements removed |

**Cancellation semantics:** a blocked `Pipe` call can be cancelled with
`IKernel::CancelTaskWait(ITask*)` or `stk::SleepCancel(TId)`. Both routes unlink the task
from the internal `ConditionVariable` wait list and the `*Ex` call returns
`WAIT_RESULT_CANCELED`.

- A cancelled `Write` stores nothing; a cancelled `Read` returns no element.
- Bulk calls keep their **partial progress**: elements already transferred before the
  cancel stay transferred and are reported through the count out-parameter.
- `ReadBulkTriggeredEx` treats a cancel like a timeout for draining: whatever is
  currently in the pipe (up to `max_count`) is drained and returned, but the result is
  `WAIT_RESULT_CANCELED`, not `WAIT_RESULT_TIMEOUT`.
- **Signal wins:** if the waiter was already signaled (removed from the wait list) when
  the cancel arrives, the cancel is a no-op and the call returns `WAIT_RESULT_SIGNAL`.
- A cancel leaves no stale wait entry: a later `Write` / `Read` is neither lost nor
  delivered twice.

**`ReadBulkTriggered` — trigger clamping:** if `trigger > max_count`, trigger is
clamped to `max_count` internally, ensuring the wait condition is always satisfiable.

**`Reset` — reader contract:** `Reset()` wakes blocked producers via `m_cv_not_full`
but does not signal `m_cv_not_empty`. A reader blocked in `ReadBulk` or
`ReadBulkTriggered` will remain blocked until its own timeout expires or data arrives.
Callers that need guaranteed unblocking must use a finite timeout.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

---

## Test Configuration

### `PipeT` tests (tests 1–8)

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_PIPE_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_PIPE_TEST_TIMEOUT` | `300` ticks | Blocking timeout for calls that must succeed |
| `_STK_PIPE_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_PIPE_TEST_LONG_SLEEP` | `100` ticks | Sleep used by verifier tasks to wait for workers |
| `_STK_PIPE_CAPACITY` | `8` | Pipe capacity (elements) used by all tests |
| `_STK_PIPE_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestPipe` (`PipeT<int32_t, 8>`) is **reconstructed in-place** via placement-new
inside `ResetTestState()` before each test, resetting all internal state including both
`ConditionVariable` instances.

### Raw `Pipe` tests (tests 9–16)

Raw `Pipe` tests live in `namespace stk::test::rawpipe` and use a dedicated
`g_RawKernel` (same template parameters as `g_Kernel`) and `g_RawPipe` constructed
over a static `uint8_t g_RawBuf[RAW_CAPACITY * RAW_ELEM_SIZE]`.

| Constant | Value | Purpose |
|----------|-------|---------|
| `RAW_ELEM_SIZE` | `4` bytes (`sizeof(RawElem)`) | Element size; exercises `element_size > 1` in all copy paths |
| `RAW_CAPACITY` | `8` (= `_STK_PIPE_CAPACITY`) | Pipe capacity in elements |

`RawElem` is a plain `struct { int32_t value; }`. Using a struct rather than a bare
`int32_t` ensures the ring-buffer arithmetic and `DrainLocked` memcpy paths are
exercised with a non-trivial element size.

`g_RawPipe` is **reconstructed in-place** via placement-new inside
`ResetRawPipeState()` before each raw test, identical in principle to `ResetTestState`.

`NeedsExtendedTasks` excludes all single-task and two-task tests (1–6, 9–16) and all
`RawCancel*` tests (17–31) from tasks 3–4 (the cancel tests match on the `"RawCancel"`
name prefix). Tests 7 and 8 always use all five tasks.

### Cancellation tests (tests 17–31)

Cancellation tests also live in `namespace stk::test::rawpipe`, run on `g_RawKernel` with
`g_RawPipe`, and are all driven by one task class, `RawCancelWaitTask`. They share the
following state, reset before each test:

| Variable | Purpose |
|----------|---------|
| `g_Victim` / `g_VictimTid` | Task whose wait is cancelled (task 1), as `ITask*` and as `TId` (for `SleepCancel`) |
| `g_VictimResult[2]` | `EWaitResult` of task 1 / task 2 (`-100` = not set) |
| `g_VictimElapsed[2]` | Milliseconds spent inside the blocking `Pipe` call by task 1 / task 2 |
| `g_VictimValue[2]` | Single element received by task 1 / task 2 (`-1` = nothing) |
| `g_VictimCount` | Elements transferred by a bulk call of task 1 (`-1` = not set) |
| `g_VictimBulk[RAW_CAPACITY]` | Elements received by a bulk read of task 1 |
| `g_VictimSleepElapsed` | Milliseconds task 1 spent in `Sleep()` after its wait was cancelled |
| `g_CancelParam` | Scenario selector (see below) |

**Scenario selection.** `RunRawTest()` declares tasks 0–2 `static`, so each is constructed
only once per template instantiation; a constructor argument would silently keep the value
of the first case. The scenario is therefore passed through the global `g_CancelParam`,
set by `main()` before every `RunRawTest<RawCancelWaitTask>()` call. Values `1–9` select
the scenario; adding `CW_VIA_SLEEPCANCEL` (`100`) makes the canceller use
`stk::SleepCancel(g_VictimTid)` instead of `g_RawKernel.CancelTaskWait(g_Victim)`.

| Constant | Value | Scenario |
|----------|-------|----------|
| `CW_READ_INFINITE` | 1 | Cancel an infinite `ReadEx` on an empty pipe |
| `CW_READ_NOT_TIMEOUT` | 2 | Cancel is reported as `CANCELED`, not `TIMEOUT` |
| `CW_WRITE_FULL` | 3 | Cancel a `WriteEx` on a full pipe |
| `CW_NO_STALE_WAITER` | 4 | Cancel leaves no stale waiter behind |
| `CW_TWO_WAITERS` | 5 | Cancel one of two blocked readers |
| `CW_SIGNAL_WINS` | 6 | Element delivered first wins over a late cancel |
| `CW_READ_BULK_PARTIAL` | 7 | Cancel of `ReadBulkEx` keeps the partial count |
| `CW_TRIGGERED_PARTIAL` | 8 | Cancel of `ReadBulkTriggeredEx` below trigger drains what is available |
| `CW_WRITE_BULK_PARTIAL` | 9 | Cancel of `WriteBulkEx` keeps the partial count |
| `CW_VIA_SLEEPCANCEL` | 100 | Added to a scenario value: cancel via `stk::SleepCancel()` |

**Task roles.** Task 0 is the canceller/checker, task 1 is the victim (sleeps
`_STK_PIPE_TEST_SHORT_SLEEP` = 10 ms, then blocks inside a `Pipe` call and records how
the wait ended), task 2 is a second reader used only by the two-waiters scenario.
Timeline: victim blocks at ~10 ms, canceller acts at ~60 ms (partial-read scenarios
deliver 3 elements at ~30 ms first). The victim's wait is infinite except in
`CW_READ_NOT_TIMEOUT` (300 ms).

`CancelVictim(bool use_sleep_cancel)` selects the cancel route.
`ReleaseStuckWaiters()` is the **safety net**: if the pass condition is not met, it pushes
elements through the pipe in both directions so that any still-blocked reader or writer is
released. A broken cancel is therefore reported as a failed test instead of hanging the
kernel (which only stops when all tasks exit).

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link the full
set of task class templates simultaneously. Tests 1–7, 9–16 and the cancellation tests 17–31 are skipped on M0;
only `StressTest` (test 8) runs, under `#ifndef __ARM_ARCH_6M__`.

`StressTest` runs on M0 because it uses a single task class template (`StressTestTask`)
instantiated for all five task slots, fitting within the available memory. The
cancellation tests add one more task class template (`RawCancelWaitTask`) and are guarded
the same way.

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
into `g_SharedCounter`. After all tasks finish, task 4 reads `g_TestPipe.GetCount()` for
any elements still in the pipe. The invariant `g_SharedCounter + remaining >= 0` confirms
that total reads never exceeded total writes — which would indicate data corruption or
a double-read.

**Pass condition:** `g_SharedCounter + g_TestPipe.GetCount() >= 0`

---

## Raw `Pipe` Tests (tests 9–16)

These tests exercise `stk::sync::Pipe` directly, using a `uint8_t` backing buffer and
`element_size = sizeof(RawElem) = 4`. All tasks live in `namespace stk::test::rawpipe`
and run on `g_RawKernel`. Helper functions `WriteElem` / `TryWriteElem` / `ReadElem`
wrap the `void*` API to keep test bodies readable.

---

### Test 9 — `RawBasicWriteRead`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Param:** `iterations = 20`

Mirrors test 1 for the raw `Pipe` API. Task 0 writes `RawElem{0}..RawElem{19}`
sequentially via `Write(&elem, timeout)`. Task 1 reads them back and verifies each
`value` field matches its expected sequence number. Confirms that `Write`/`Read` with
a 4-byte struct element size transfers data correctly and preserves FIFO ordering.

**Pass condition:** `counter == 20`

---

### Test 10 — `RawBulkWriteRead`
**Tasks:** 0–2 only

Mirrors test 5 for the raw `Pipe` API. Task 0 builds `RawElem src[8] = {{0}..{7}}`
and writes it via `WriteBulk(src, 8, timeout)`. Task 1 calls `ReadBulk(dst, 8, timeout)`
and verifies the returned count equals `8` and every `value` field matches its sequence
number. Exercises the ring-buffer two-part `memcpy` path with a non-unit element size.

**Pass condition:** `written == 8` and `counter == 8`

---

### Test 11 — `RawTriggeredSatisfied`
**Tasks:** 0–2 only

Task 1 (consumer) calls `ReadBulkTriggered(dst, trigger=4, max_count=8, timeout=300)`
on an empty pipe and blocks. Task 2 (producer) sleeps `_STK_PIPE_TEST_SHORT_SLEEP`
then writes all 8 elements in one `WriteBulk`. The consumer must unblock — because
`m_count` reaches 4, satisfying the trigger — and drain all 8 elements in one atomic
critical-section pass. Verifies the core trigger-wait path and the post-trigger
full-drain behaviour.

**Pass condition:** `n >= trigger` (4) and all `n` values correct; `counter == 8`

---

### Test 12 — `RawTriggeredTimeoutPartial`
**Tasks:** 0–2 only

Task 1 calls `ReadBulkTriggered(dst, trigger=6, max_count=8, timeout=80)`. Task 2
writes only 3 elements (below the trigger threshold) after a short delay. The trigger
is never reached; the 80-tick timeout fires. The consumer must return exactly 3 — the
bytes that arrived before the deadline — proving the FreeRTOS partial-timeout drain
semantics: when timeout fires, whatever is in the pipe is drained and returned rather
than discarded.

**Pass condition:** `n == 3` and all 3 values correct; `counter == 3`

---

### Test 13 — `RawTriggeredNoWait`
**Tasks:** 0–2 only

Task 1 pre-loads 3 elements via `TryWrite` (no producer task needed), then calls
`ReadBulkTriggered(dst, trigger=6, max_count=8, NO_WAIT)`. With `NO_WAIT` the trigger
threshold is ignored: the call must return immediately with 3 elements. Elapsed time is
measured and must be under 5 ms, proving the `NO_WAIT` fast-path bypasses the CV wait
entirely regardless of the trigger value.

**Pass condition:** `n == 3`, `elapsed < 5 ms`, all values correct; `counter == 3`

---

### Test 14 — `RawTriggeredClamp`
**Tasks:** 0–2 only

Task 1 calls `ReadBulkTriggered(dst, trigger=8, max_count=2, timeout=300)`. The trigger
(8) exceeds `max_count` (2) and is clamped to 2 internally, making the wait condition
satisfiable. Task 2 writes exactly 2 elements. The consumer must unblock and return
those 2 elements. Without clamping, `trigger > max_count` would create a condition that
can never be satisfied, blocking the reader indefinitely.

**Pass condition:** `n == 2`, values `{10, 20}` correct; `counter == 2`

---

### Test 15 — `RawResetDoesNotReleaseReader`
**Tasks:** 0–2 only

Verifies that `Pipe::Reset()` matches FreeRTOS `xStreamBufferReset()` semantics: it
does **not** unblock a blocked reader. Task 1 calls `ReadBulkTriggered(dst, trigger=8,
max_count=8, timeout=80)` on an empty pipe. Task 2 calls `Reset()` after
`_STK_PIPE_TEST_SHORT_SLEEP` (10 ms). The reader must remain blocked for the full 80
ticks — `Reset()` only signals `m_cv_not_full` (waking producers), never
`m_cv_not_empty`. The consumer finally times out and returns 0. Elapsed time is
measured to confirm `Reset()` did not cause an early return.

**Pass condition:** `n == 0`, `elapsed >= 75 ms`, `IsEmpty() == true`, `GetCount() == 0`; `counter == 1`

---

### Test 16 — `RawStateInvariants`
**Tasks:** 0–2 only (task 1 is the sole active worker)

Single-task invariant check across a complete fill-and-drain cycle, mirroring test 6
for the raw `Pipe` API and additionally checking `GetCapacity`, `GetElementSize`,
`GetSpace`, and `IsFull`. Task 1 asserts: construction-time properties are immutable;
`IsEmpty`/`GetCount`/`GetSpace` are correct on an empty pipe; after each `TryWrite`,
`GetCount` equals `i + 1` and `GetSpace` equals `CAPACITY - i - 1`; after filling,
`IsFull` is true and `GetSpace` is 0; after each `TryRead`, counts track back down;
after full drain, `IsEmpty` is true and `GetCount` is 0.

**Pass condition:** `counter == 1` (all assertions passed)

---

## Cancellation Tests (tests 17–31)

These tests verify that a task blocked inside a `Pipe` call can be cancelled, and that the
cancel is clean: the right `EWaitResult` is reported, partial transfers are preserved, no
element is lost or duplicated, and the wait list stays consistent. `Pipe` builds on
`ConditionVariable`, so they also cover the cancel path of `ConditionVariable::WaitEx()`.
Shared state, scenario selection and task roles are described under
[Test Configuration](#cancellation-tests-tests-17–31). Tests 17–25 cancel through
`IKernel::CancelTaskWait()`; tests 26–31 repeat the key scenarios through
`stk::SleepCancel()`.

> **Safety net:** if a cancel fails, the victim stays blocked and the kernel would never
> stop. On any failed pass condition the canceller calls `ReleaseStuckWaiters()` (writes
> and reads through the pipe), so a broken cancel shows up as a failed test, not a hang.
>
> **Kernel requirement:** cancellation requires the `KERNEL_DYNAMIC | KERNEL_SYNC` kernel
> mode used by all tests in this suite.

All tests use tasks 0–2 only.

---

### Test 17 — `RawCancel_ReadInfinite`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `1`

Task 1 calls `ReadEx(&e, WAIT_INFINITE)` on an empty pipe at ~10 ms and blocks. Task 0 calls `CancelTaskWait(g_Victim)` at ~60 ms. The read must return `WAIT_RESULT_CANCELED` after roughly 50 ms, with no element received and the pipe still empty. Basic cancel of a reader parked on `m_cv_not_empty`.

**Pass condition:** `result == CANCELED`, `elapsed ∈ [35, 75] ms`, `value == -1`, `IsEmpty()`

---

### Test 18 — `RawCancel_ReadNotTimeout`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `2`

Same as test 17 but the victim uses a finite 300 ms timeout. The cancel at ~60 ms must end the wait early and be reported as `CANCELED`, not `TIMEOUT`. Guards against the cancel path being conflated with the timeout path in `ReadEx`.

**Pass condition:** `result == CANCELED`, `elapsed < 150 ms`

---

### Test 19 — `RawCancel_WriteFull`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `3`

Task 0 pre-loads `0..7` so the pipe is full; task 1 calls `WriteEx(&{999}, WAIT_INFINITE)` and blocks on `m_cv_not_full`. After the cancel the write must return `CANCELED`, the pipe must still be full, and draining it must yield exactly `0..7` — the cancelled write stored nothing.

**Pass condition:** `result == CANCELED`, `elapsed ∈ [35, 75] ms`, `IsFull()`, FIFO content `0..7` intact, pipe empty after drain

---

### Test 20 — `RawCancel_ReadNoStaleWaiter`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `4`

After the victim's read is cancelled, task 1 sleeps 60 ms and records the real duration. Task 0 then writes `321` with nobody waiting, reads it back with `NO_WAIT`, and polls once more. The element must stay in the pipe (not lost to a ghost waiter), be consumed exactly once, and the victim's sleep must not be cut short by a stale wait entry.

**Pass condition:** `result == CANCELED`, write ok, first poll returns `321`, second poll empty, `victim_sleep >= 55 ms`

---

### Test 21 — `RawCancel_ReadTwoWaiters`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `5`

Task 1 and, 10 ms later, task 2 both block in `ReadEx(WAIT_INFINITE)`. Task 0 cancels only task 1, then writes `77`. The cancel must leave the wait list consistent: task 2 is woken by the write with `SIGNAL` and receives `77`; task 1 stays `CANCELED` with no value.

**Pass condition:** task 1 `CANCELED`, `value[0] == -1`; task 2 `SIGNAL`, `value[1] == 77`; pipe empty

---

### Test 22 — `RawCancel_ReadSignalWins`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `6`

Task 0 calls `TryWrite(777)` and `CancelTaskWait` back to back with no scheduling point between them. The write already signals the reader and removes it from the wait list, so the cancel must be a no-op. The reader returns `SIGNAL` with the element `777`; it is not cancelled and the element is not lost.

**Pass condition:** write ok, `result == SIGNAL`, `value == 777`, pipe empty

---

### Test 23 — `RawCancel_ReadBulkPartial`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `7`

Task 1 calls `ReadBulkEx(bulk, 8, n, WAIT_INFINITE)`. At ~30 ms task 0 writes 3 elements `{0,1,2}`; at ~60 ms it cancels. The call must return `CANCELED` with `n == 3` and `bulk[0..2] == {0,1,2}` — the elements read before the cancel are kept, not discarded — and the pipe is empty.

**Pass condition:** `result == CANCELED`, `count == 3`, values `{0,1,2}`, `elapsed ∈ [35, 75] ms`, pipe empty

---

### Test 24 — `RawCancel_ReadTriggeredPartial`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `8`

Task 1 calls `ReadBulkTriggeredEx(bulk, trigger=6, max_count=8, n, WAIT_INFINITE)`. Only 3 elements arrive (below the trigger) before the cancel at ~60 ms. As on timeout (test 12) the 3 available elements must still be drained, but the result is `CANCELED`, not `TIMEOUT`.

**Pass condition:** `result == CANCELED`, `count == 3`, values `{0,1,2}`, `elapsed ∈ [35, 75] ms`, pipe empty

---

### Test 25 — `RawCancel_WriteBulkPartial`
**Cancel via:** `CancelTaskWait` &nbsp;|&nbsp; **`g_CancelParam`:** `9`

Task 0 pre-loads 6 of 8 slots (`0..5`). Task 1 calls `WriteBulkEx(bulk, 8, n, WAIT_INFINITE)` with values `50..57`: 2 elements fit (`50`, `51`), then it blocks on the full pipe. After the cancel `n` must be `2`, the result `CANCELED`, and the pipe must contain `0..5, 50, 51` in order — the written prefix is kept, the rest is dropped.

**Pass condition:** `result == CANCELED`, `written == 2`, `elapsed ∈ [35, 75] ms`, `IsFull()`, FIFO content `{0..5, 50, 51}`

---

### Test 26 — `RawCancel_SleepCancelRead`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `101`

Identical scenario to test 17 (`ReadInfinite`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 17

---

### Test 27 — `RawCancel_SleepCancelWrite`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `103`

Identical scenario to test 19 (`WriteFull`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 19

---

### Test 28 — `RawCancel_SleepCancelNoStale`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `104`

Identical scenario to test 20 (`ReadNoStaleWaiter`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 20

---

### Test 29 — `RawCancel_SleepCancelTwo`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `105`

Identical scenario to test 21 (`ReadTwoWaiters`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 21

---

### Test 30 — `RawCancel_SleepCancelSignal`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `106`

Identical scenario to test 22 (`ReadSignalWins`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 22

---

### Test 31 — `RawCancel_SleepCancelTriggered`
**Cancel via:** `SleepCancel` &nbsp;|&nbsp; **`g_CancelParam`:** `108`

Identical scenario to test 24 (`ReadTriggeredPartial`), but the canceller calls `stk::SleepCancel(g_VictimTid)` instead of `CancelTaskWait()`. Verifies that `SleepCancel` routes through `OnTaskSleepCancel` into the wait object's `Cancel()` when the task is blocked in a `Pipe` call, with the same outcome as the direct kernel call.

**Pass condition:** same as test 24

---

## Summary Table

| # | Test | Tasks | Pass condition | What it verifies |
|---|------|-------|----------------|------------------|
| 1 | `BasicWriteReadTask` | 0–2 | `counter == 20` | Single-element `Write()` / `Read()` transfers values correctly in FIFO order |
| 2 | `WriteBlocksWhenFullTask` | 0–2 | `counter == 9` | `Write()` blocks when pipe is at capacity; unblocks atomically when consumer frees a slot |
| 3 | `ReadBlocksWhenEmptyTask` | 0–2 | `counter == 1` | `Read()` blocks on an empty pipe; unblocks when producer writes; received value matches |
| 4 | `TimeoutTask` | 0–2 | `counter == 2` | `Read()` on empty and `Write()` on full both return `false` within `[45, 65]` ms |
| 5 | `BulkWriteReadTask` | 0–2 | `written == 8`, `counter == 8` | `WriteBulk()` / `ReadBulk()` transfers a full block; count and all element values correct |
| 6 | `GetSizeIsEmptyTask` | 0–2 | `counter == 1` | `GetCount()` tracks exactly after every `Write()` and `Read()`; `IsEmpty()` correct before and after |
| 7 | `MultiProducerConsumerTask` | 0–4 | `counter == 40` | Two concurrent producers and two concurrent consumers transfer all items without loss or duplication |
| 8 | `StressTestTask` | 0–4 | `net + remaining >= 0` | No data corruption under full five-task contention mixing blocking writes and reads; runs on all platforms |
| 9 | `RawBasicWriteReadTask` | 0–2 | `counter == 20` | `Pipe::Write()` / `Read()` transfers a 4-byte `RawElem` struct in FIFO order |
| 10 | `RawBulkWriteReadTask` | 0–2 | `written == 8`, `counter == 8` | `Pipe::WriteBulk()` / `ReadBulk()` transfers a full block; `memcpy` with `element_size=4` correct |
| 11 | `RawTriggeredSatisfiedTask` | 0–2 | `n >= 4`, `counter == 8` | `ReadBulkTriggered()` blocks until trigger met; drains full available batch atomically |
| 12 | `RawTriggeredTimeoutPartialTask` | 0–2 | `counter == 3` | Timeout before trigger fires partial drain of bytes that arrived; nothing is discarded |
| 13 | `RawTriggeredNoWaitTask` | 0–2 | `counter == 3`, `elapsed < 5 ms` | `NO_WAIT` bypasses trigger and returns immediately with available data |
| 14 | `RawTriggeredClampTask` | 0–2 | `counter == 2` | `trigger > max_count` is clamped to `max_count`; wait is always satisfiable |
| 15 | `RawResetDoesNotReleaseReaderTask` | 0–2 | `counter == 1`, `elapsed >= 75 ms` | `Reset()` does not unblock blocked readers; reader waits full timeout (matches FreeRTOS semantics) |
| 16 | `RawStateInvariantsTask` | 0–2 | `counter == 1` | `GetCapacity` / `GetElementSize` / `GetCount` / `GetSpace` / `IsEmpty` / `IsFull` track exactly throughout fill and drain |
| 17 | `RawCancel_ReadInfinite` | 0–2 | `CANCELED`, `elapsed ∈ [35, 75] ms` | `CancelTaskWait` ends an infinite `ReadEx` on an empty pipe |
| 18 | `RawCancel_ReadNotTimeout` | 0–2 | `CANCELED`, `elapsed < 150 ms` | Cancel is reported as `CANCELED`, not `TIMEOUT` |
| 19 | `RawCancel_WriteFull` | 0–2 | `CANCELED`, content `0..7` intact | Cancelled `WriteEx` on a full pipe stores nothing |
| 20 | `RawCancel_ReadNoStaleWaiter` | 0–2 | `CANCELED`, element read once, `sleep >= 55 ms` | No stale waiter left: next element neither lost nor consumed twice |
| 21 | `RawCancel_ReadTwoWaiters` | 0–2 | 1st `CANCELED`, 2nd `SIGNAL` with `77` | Cancelling one of two blocked readers leaves the other intact |
| 22 | `RawCancel_ReadSignalWins` | 0–2 | `SIGNAL`, `value == 777` | Cancel after the waiter was already signaled is a no-op |
| 23 | `RawCancel_ReadBulkPartial` | 0–2 | `CANCELED`, `count == 3` | `ReadBulkEx` keeps elements read before the cancel |
| 24 | `RawCancel_ReadTriggeredPartial` | 0–2 | `CANCELED`, `count == 3` | `ReadBulkTriggeredEx` below trigger drains available data, reports `CANCELED` |
| 25 | `RawCancel_WriteBulkPartial` | 0–2 | `CANCELED`, `written == 2`, content `0..5,50,51` | `WriteBulkEx` keeps the prefix written before the cancel |
| 26 | `RawCancel_SleepCancelRead` | 0–2 | as test 17 | `SleepCancel` cancels a blocked `ReadEx` |
| 27 | `RawCancel_SleepCancelWrite` | 0–2 | as test 19 | `SleepCancel` cancels a blocked `WriteEx` |
| 28 | `RawCancel_SleepCancelNoStale` | 0–2 | as test 20 | `SleepCancel` leaves no stale waiter |
| 29 | `RawCancel_SleepCancelTwo` | 0–2 | as test 21 | `SleepCancel` of one of two readers leaves the other intact |
| 30 | `RawCancel_SleepCancelSignal` | 0–2 | as test 22 | `SleepCancel` after signal is a no-op |
| 31 | `RawCancel_SleepCancelTriggered` | 0–2 | as test 24 | `SleepCancel` of `ReadBulkTriggeredEx` drains available data, reports `CANCELED` |
