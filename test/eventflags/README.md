# EventFlags Test Suite

Test suite for `stk::sync::EventFlags`.  
Source: `test/eventflags/test_eventflags.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_eventflags.h>
```

`EventFlags` is a 32-bit multi-flag synchronization primitive. Each bit in the
internal flags word represents an independent boolean event. Tasks can set or clear
any combination of bits and block until any one (OR semantics) or all (AND semantics)
of a requested subset become set.

Unlike `Event`, which models a single binary signal, `EventFlags` coordinates up to
31 independent conditions in a single object. Bit 31 is reserved for error sentinels
and must never be set by the caller.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::EventFlags g_Flags;           // all flags clear (default)
stk::sync::EventFlags g_Flags(FLAG_A);  // constructed with FLAG_A pre-set
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Set` | `uint32_t Set(uint32_t flags)` | Atomically OR-sets the specified bits; wakes **all** current waiters so each can re-evaluate its predicate. Returns the flags word **after** setting. Returns `ERROR_PARAMETER` if `flags` is 0 or has bit 31 set. ISR-safe. |
| `Clear` | `uint32_t Clear(uint32_t flags)` | Atomically clears the specified bits. Returns the flags word **before** clearing. Returns `ERROR_PARAMETER` if `flags` is 0 or has bit 31 set. ISR-safe. |
| `Get` | `uint32_t Get() const` | Non-destructive point-in-time snapshot of the flags word. Never blocks or modifies state. ISR-safe. |
| `Wait` | `uint32_t Wait(uint32_t flags, uint32_t options = OPT_WAIT_ANY, Timeout timeout = WAIT_INFINITE)` | Blocks until the flag condition is satisfied or the timeout expires. On success, clears matched bits unless `OPT_NO_CLEAR` is set. Returns the matched bitmask on success, or an `ERROR_*` sentinel on failure. ISR-safe only with `timeout = NO_WAIT`. |
| `TryWait` | `uint32_t TryWait(uint32_t flags, uint32_t options = OPT_WAIT_ANY)` | Non-blocking poll; equivalent to `Wait(flags, options, NO_WAIT)`. Returns matched bitmask on success, or `ERROR_TIMEOUT` immediately if the condition is not met. Never blocks. ISR-safe. |
| `IsError` | `static bool IsError(uint32_t result)` | Returns `true` if the return value carries an error sentinel (bit 31 set). Use after every `Set()`, `Clear()`, and `Wait()` call. |
| `~EventFlags` | (destructor) | Asserts no dangling waiters via the internal `Mutex` and `ConditionVariable` destructors in debug builds. |

**Options bitmask (passed to `Wait()` / `TryWait()`):**

| Constant | Value | Meaning |
|----------|-------|---------|
| `OPT_WAIT_ANY` | `0x00000000` | Unblock when **any** requested bit is set (OR semantics, default) |
| `OPT_WAIT_ALL` | `0x00000001` | Unblock only when **all** requested bits are simultaneously set (AND semantics) |
| `OPT_NO_CLEAR` | `0x00000002` | Do not clear matched bits on a successful return |

Options may be combined: `OPT_WAIT_ALL | OPT_NO_CLEAR`.

**Error sentinels returned by `Set()`, `Clear()`, `Wait()`, `TryWait()`:**

| Constant | Value | Meaning |
|----------|-------|---------|
| `ERROR_PARAMETER` | `0x80000001` | `flags` argument is 0 or has bit 31 set |
| `ERROR_TIMEOUT` | `0x80000002` | Timeout expired before the flag condition was met |
| `ERROR_ISR` | `0x80000004` | `Wait()` called from an ISR with a blocking timeout |
| `ERROR_CANCELED` | `0x80000008` | Wait was cancelled via `IKernel::CancelTaskWait()` or `stk::SleepCancel()` before the flag condition was met; never reported as `ERROR_TIMEOUT` |
| `ERROR_FAIL` | `0x80000010` | Kernel rejected the wait (e.g. `KERNEL_SYNC` mode not enabled) |
| `ERROR_MASK` | `0x80000000` | Mask for testing any error; bit 31 set means error |

**Behavior by option:**

| Operation | `OPT_WAIT_ANY` (default) | `OPT_WAIT_ALL` |
|-----------|----------------------|------------|
| `Wait()` unblocks when | Any one requested bit is set | Every requested bit is simultaneously set |
| Matched bits cleared (default) | The bits that triggered the wake | All requested bits |
| `OPT_NO_CLEAR` effect | Matched bits stay set; next `Wait()` fast-paths | All requested bits stay set; next `Wait()` fast-paths |
| `TryWait()` not satisfied | Returns `ERROR_TIMEOUT` immediately | Returns `ERROR_TIMEOUT` immediately |
| `Set()` with waiters | Wakes **all** waiters; each re-evaluates its predicate | Same |
| `Set()` with no waiters | Bits stay set; next `Wait()` fast-paths | Same |
| `Clear()` | Returns pre-clear word; blocked `OPT_WAIT_ANY` waiters for cleared bits will time out | Same |
| Wait cancelled | Returns `ERROR_CANCELED`; flags word untouched | Same; bits set by a partial `Set()` stay set |

**Key invariants:**

- `Set()` always wakes **all** current waiters. Each waiter independently decides
  whether its own predicate is now satisfied, then clears only its matched bits.
  Concurrent waiters with disjoint flag masks do not interfere with each other.
- `Clear()` returns the flags word **before** the clear; the caller can determine
  which bits were actually set at the time of the call.
- `Get()` is purely observational. It never blocks, never modifies state, and never
  clears any bit. A subsequent `Wait()` for bits observed by `Get()` will still
  succeed if those bits have not been cleared by another task in the interim.
- `OPT_WAIT_ALL` re-evaluates the full predicate each time it is woken, guarding against
  spurious wakeups and races where a bit is set and cleared between two `Set()` calls.
- A cancelled wait returns `ERROR_CANCELED` immediately, regardless of the deadline, and
  does not modify the flags word. It leaves no stale waiter behind and does not affect
  other waiters. If a `Set()` already woke the waiter before the cancel arrives, the
  cancel is a no-op and the wait succeeds normally.
- Destroying an `EventFlags` object while tasks are waiting is a logic error; an
  assertion fires in debug builds via the internal `Mutex` and `ConditionVariable`
  destructors.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_EF_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_EF_TEST_TIMEOUT` | `300` ticks | Blocking timeout for `Wait()` calls that must succeed |
| `_STK_EF_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_EF_TEST_LONG_SLEEP` | `100` ticks | Sleep used by the verifier task to wait for workers |
| `_STK_EF_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_Flags` is **reconstructed in-place** via placement-new inside `ResetTestState()`
before each test. `ResetTestState()` accepts an `initial_flags` parameter forwarded
directly to the `EventFlags` constructor, so the same helper serves all 22 tests
without any additional per-test setup code. It also resets the cancellation state
(see below).

Cancellation tests (11–22) use additional shared state, all cleared by `ResetTestState()`:

| Variable | Purpose |
|----------|---------|
| `g_Victim` / `g_VictimTid` | Task (as `ITask *` and as `TId`) whose wait is cancelled; published by task 1 just before it blocks |
| `g_VictimResult[2]` | Value returned by `Wait()` in task 1 / task 2 (`0` = not set; `Wait()` never returns 0) |
| `g_VictimElapsed[2]` | Milliseconds spent inside `Wait()` by task 1 / task 2 |
| `g_VictimSleepElapsed` | Milliseconds task 1 spent in `Sleep(60)` after its wait ended (`NoStaleWaiter` only) |

`CancelVictim(use_sleep_cancel)` cancels the victim's wait either via
`g_Kernel.CancelTaskWait(g_Victim)` or via `stk::SleepCancel(g_VictimTid)`. The
scenario is selected by the `param` of `RunTest()`; adding `CW_VIA_SLEEPCANCEL` (`100`)
switches the cancel path to `stk::SleepCancel()`.

Shared flag bit definitions used across all tests:

```cpp
static const uint32_t FLAG_A = (1U << 0);
static const uint32_t FLAG_B = (1U << 1);
static const uint32_t FLAG_C = (1U << 2);
static const uint32_t FLAG_D = (1U << 3);
```

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eleven
distinct task class templates simultaneously. Test 1 (`SetWaitAny`) runs on all
platforms; tests 2–22 are skipped on M0 under `#ifndef __ARM_ARCH_6M__`.

`SetWaitAny` runs on M0 because it uses a single task class template
(`SetWaitAnyTask`) instantiated for all five task slots, fitting within the available
memory. Tests 2–22 introduce distinct task class templates (tests 11–22 share one,
`CancelWaitTask`), multiplying the
static memory footprint beyond what M0 can accommodate.

| Platform | `_STK_EF_STACK_SIZE` |
|----------|-----------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `SetWaitAny`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY` &nbsp;|&nbsp; **Param:** `iterations = 20` &nbsp;|&nbsp; **Runs on M0: yes**

Task 0 is the producer; tasks 1–4 are consumers. All four consumers loop calling
`Wait(FLAG_A | FLAG_B | FLAG_C | FLAG_D, OPT_WAIT_ANY, _STK_EF_TEST_TIMEOUT)`. The
producer alternates between `FLAG_A` and `FLAG_B`, calling `Set()` 20 times with
`Delay(1)` between calls so each signal lands on exactly one blocked consumer before
the next fires. Each successful wake increments `g_SharedCounter`. Verifies that the
auto-clear behavior of `Wait()` consumes the matching bit, leaving the remaining
consumers blocked until the next `Set()`.

**Pass condition:** `counter == 20`

---

### Test 2 — `SetWaitAll`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ALL`

Task 1 blocks on `Wait(FLAG_A | FLAG_B | FLAG_C, OPT_WAIT_ALL, _STK_EF_TEST_TIMEOUT)`.
Task 0 sets the three flags one at a time with `_STK_EF_TEST_SHORT_SLEEP` delays
between each. Task 1 must remain blocked after each individual `Set()` and only wake
when all three bits are simultaneously present after the final `Set(FLAG_C)`. The
returned value must equal exactly `FLAG_A | FLAG_B | FLAG_C` with no extra bits set.

**Pass condition:** `counter == 1` and `result == (FLAG_A | FLAG_B | FLAG_C)`

---

### Test 3 — `Clear`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY`

Task 0 calls `Set(FLAG_A | FLAG_B)` then `Clear(FLAG_A)`. Verifies three properties of
`Clear()`: the return value equals the flags word **before** clearing (both bits
present); `FLAG_A` is absent from `Get()` after the call; `FLAG_B` remains set.
Task 1 then calls `Wait(FLAG_B)`, which must succeed immediately on the still-set
`FLAG_B`. Task 2 calls `Wait(FLAG_A)` with a short timeout; since `FLAG_A` was
cleared, this must time out — any non-error return is counted as a failure.

**Pass condition:** `pre_ok == true`, `post_ok == true`, `flag_b_ok == true`, `counter == 1`

---

### Test 4 — `NoClear`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY | OPT_NO_CLEAR`

Task 0 sleeps briefly to let tasks 1 and 2 reach their `Wait()` calls, then sets
`FLAG_A`. Both task 1 and task 2 call `Wait(FLAG_A, OPT_WAIT_ANY | OPT_NO_CLEAR, timeout)`.
Because neither clears the flag on return, the second waiter must also succeed on
the same bit. Verifies that `OPT_NO_CLEAR` makes a single `Set()` satisfy multiple
concurrent waiters — behaviour that would otherwise require a manual-reset `Event`.

**Pass condition:** `counter == 2`

---

### Test 5 — `Timeout`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY`

Task 1 calls `Wait(FLAG_A, OPT_WAIT_ANY, 50)` after `_STK_EF_TEST_SHORT_SLEEP`. No
producer fires within the 50-tick window so it must return `ERROR_TIMEOUT` with
measured elapsed time in `[45, 60]` ms. Task 0 withholds `Set()` until tick 200.
Task 2 sleeps until tick 210 then calls `Wait(FLAG_A, OPT_WAIT_ANY, 100)`, which must
succeed on the now-set flag. Task 2 is the verifier.

**Pass condition:** `counter == 2`  
(1 = `ERROR_TIMEOUT` returned within correct timing bounds; 2 = `Wait()` succeeded after `Set()`)

---

### Test 6 — `TryWait`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY`

Task 1 calls `TryWait(FLAG_A)` on a non-set flag; must return `ERROR_TIMEOUT`
immediately (elapsed < `_STK_EF_TEST_SHORT_SLEEP`). Task 2 calls `Set(FLAG_A)` then
`TryWait(FLAG_A)`; must return the matched bitmask immediately (event just set) and
auto-clear `FLAG_A`. Task 2 then calls `TryWait(FLAG_A)` a second time to confirm
the clear: must return `ERROR_TIMEOUT`. Covers both the non-set and set fast paths
and the auto-clear `TryWait()` performs on success.

**Pass condition:** `counter == 3`  
(1 = non-set `TryWait()` returned an error immediately; 2 = set `TryWait()` returned a match immediately; 3 = post-clear `TryWait()` returned an error)

---

### Test 7 — `Get`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY`

Task 0 calls `Get()` on an empty flags word (must be 0), sets `FLAG_A | FLAG_C`,
calls `Get()` again (must have both bits), clears `FLAG_C`, calls `Get()` a third
time (must equal exactly `FLAG_A`). Task 1 sleeps to let task 0 complete the setup,
calls `Get()` to observe `FLAG_A`, then calls `Wait(FLAG_A, OPT_WAIT_ANY, timeout)` to confirm that `Get()`
did not consume the flag — the `Wait()` must succeed immediately. Verifies that
`Get()` is purely observational at every stage.

**Pass condition:** `empty_ok == true`, `set_ok == true`, `clear_ok == true`, `counter == 1`

---

### Test 8 — `MultiWaiterAny`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY`

Each consumer (tasks 1–4) watches a unique flag bit: task 1 waits for `FLAG_A`,
task 2 for `FLAG_B`, task 3 for `FLAG_C`, task 4 for `FLAG_D`. After
`_STK_EF_TEST_SHORT_SLEEP` the producer calls `Set(FLAG_A | FLAG_B | FLAG_C | FLAG_D)`
once, which must wake all four simultaneously. Each consumer verifies that its own
expected bit is present in the returned bitmask. Demonstrates that concurrent
`OPT_WAIT_ANY` waiters with disjoint flag masks are all satisfied by a single `Set()`
without interfering with each other.

**Pass condition:** `counter == 4`

---

### Test 9 — `MultiWaiterAll`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Options:** `OPT_WAIT_ALL` &nbsp;|&nbsp; **Param:** `consumers = 4`

All four consumers (tasks 1–4) block on `Wait(FLAG_A | FLAG_B, OPT_WAIT_ALL, timeout)`.
Because the default auto-clear behavior causes the first waker to consume both bits,
the producer loops, calling `Set(FLAG_A | FLAG_B)` with `Delay(1)` between iterations
— once per consumer — to restock the flags after each take. Every consumer must
eventually wake and verify that the returned value equals `FLAG_A | FLAG_B` exactly.

**Pass condition:** `counter == 4`

---

### Test 10 — `InitialFlags`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY` &nbsp;|&nbsp; **Initial flags:** `FLAG_A`

The `EventFlags` object is constructed with `FLAG_A` pre-set via the `initial_flags`
constructor parameter. Task 1 calls `Wait(FLAG_A, OPT_WAIT_ANY, _STK_EF_TEST_SHORT_SLEEP)`
immediately; because the flag is already set, `Wait()` takes the fast path and returns
the matched mask without blocking, then auto-clears `FLAG_A`. Task 1 then calls
`Wait()` a second time; the flag has been consumed so this must return `ERROR_TIMEOUT`.
Verifies the `initial_flags` constructor parameter and the fast-path auto-clear
behavior.

**Pass condition:** `counter == 2`  
(1 = first `Wait()` succeeded immediately on pre-set flag; 2 = second `Wait()` timed out after auto-clear)

---

### Tests 11–22 — `CancelWait_*`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Task class:** `CancelWaitTask` &nbsp;|&nbsp; **Options:** `OPT_WAIT_ANY` unless noted

Verify that a blocked `Wait()` can be cancelled and returns `ERROR_CANCELED`. Task 0 is the
canceller and checker, task 1 is the victim blocked in `Wait()` from ~10 ms, and task 2 is
a second waiter used only by the two-waiters scenario. Task 0 cancels at ~60 ms. All
scenarios run through `IKernel::CancelTaskWait()`; the `SleepCancel` variants (tests 18–22)
repeat a subset through `stk::SleepCancel()` to confirm both paths behave identically.

If a cancel fails, task 0 sets `FLAG_A | FLAG_B` three times so any still-blocked waiter is
released and the test fails instead of hanging the kernel.

#### Test 11 — `CancelWait_Infinite`
Task 1 calls `Wait(FLAG_A, OPT_WAIT_ANY, WAIT_INFINITE)` and is cancelled by task 0.

**Pass condition:** result is `ERROR_CANCELED` and elapsed time is in `[35, 75]` ms.

#### Test 12 — `CancelWait_NotTimeout`
Task 1 waits with a 300 ms timeout and is cancelled long before it expires.

**Pass condition:** result is `ERROR_CANCELED` (not `ERROR_TIMEOUT`) and elapsed time is below 150 ms.

#### Test 13 — `CancelWait_NoStaleWaiter`
After the cancel, task 0 calls `Set(FLAG_A)` with nobody waiting, then `TryWait(FLAG_A)` twice.
Task 1 meanwhile sleeps 60 ms; a ghost wait entry left by the cancel would wake it early.

**Pass condition:** result is `ERROR_CANCELED`; `Set()` returns `FLAG_A`; first `TryWait()` returns
`FLAG_A`; second returns an error; task 1 slept at least 55 ms.

#### Test 14 — `CancelWait_FlagsUntouched`
**Initial flags:** `FLAG_B`

Task 1 waits for `FLAG_A` and is cancelled. Task 0 then checks that the cancel did not modify
the flags word, and that `Set()`, `OPT_NO_CLEAR` and auto-clear still work correctly afterwards.

**Pass condition:** result is `ERROR_CANCELED`; `Get()` after the cancel is `FLAG_B`; `TryWait(FLAG_A)` fails;
`Set(FLAG_A)` returns `FLAG_A | FLAG_B`; a `NO_CLEAR` poll returns `FLAG_A` and leaves both bits set; a
normal poll returns `FLAG_A`; the final word is `FLAG_B`.

#### Test 15 — `CancelWait_TwoWaiters`
Tasks 1 and 2 both wait for `FLAG_A`; only task 1 is cancelled. Task 0 then calls `Set(FLAG_A)`.

**Pass condition:** task 1 gets `ERROR_CANCELED`; task 2 gets `FLAG_A`; a following `TryWait(FLAG_A)` fails
(task 2 consumed the flag).

#### Test 16 — `CancelWait_SignalWins`
Task 0 calls `Set(FLAG_A)` and `CancelTaskWait()` back to back with no scheduling point between them.
The waiter has already been woken, so the cancel must be a no-op.

**Pass condition:** task 1 gets `FLAG_A` (not an error) and a following `TryWait(FLAG_A)` fails.

#### Test 17 — `CancelWait_WaitAllPartial`
**Options:** `OPT_WAIT_ALL`

Task 1 waits for `FLAG_A | FLAG_B`. Task 0 sets only `FLAG_A`: task 1 wakes, finds the condition
unmet and blocks again. Task 0 then cancels it. Verifies that a cancel is honored after a
re-evaluation wakeup and that the partial match is not consumed.

**Pass condition:** task 1 is still blocked after the partial `Set()`; result is `ERROR_CANCELED`; elapsed
time is in `[45, 85]` ms; `Get()` is `FLAG_A`.

#### Tests 18–22 — `CancelWait_SleepCancel*`
Same scenarios as tests 11, 13, 15, 16 and 17, but cancelled through `stk::SleepCancel(tid)`:

| # | Test | Repeats |
|---|------|---------|
| 18 | `CancelWait_SleepCancel` | Test 11 (`Infinite`) |
| 19 | `CancelWait_SleepCancelNoStale` | Test 13 (`NoStaleWaiter`) |
| 20 | `CancelWait_SleepCancelTwo` | Test 15 (`TwoWaiters`) |
| 21 | `CancelWait_SleepCancelSignal` | Test 16 (`SignalWins`) |
| 22 | `CancelWait_SleepCancelAllPartial` | Test 17 (`WaitAllPartial`) |

**Pass condition:** same as the corresponding test above.

---

## Summary Table

| # | Test | Tasks | Options | Pass condition | What it verifies |
|---|------|-------|---------|----------------|------------------|
| 1 | `SetWaitAny` | 0–4 | `OPT_WAIT_ANY` | `counter == 20` | `Set()` unblocks exactly one `OPT_WAIT_ANY` waiter per call via auto-clear; runs on all platforms including M0 |
| 2 | `SetWaitAll` | 0–2 | `OPT_WAIT_ALL` | `counter == 1`, `result == FLAG_A\|FLAG_B\|FLAG_C` | `Wait()` holds until **all** requested bits are simultaneously present; returned mask equals the full request |
| 3 | `Clear` | 0–2 | `OPT_WAIT_ANY` | `pre/post/flag_b_ok == true`, `counter == 1` | `Clear()` returns the pre-clear word; cleared bits block subsequent waiters; un-cleared bits remain available |
| 4 | `NoClear` | 0–2 | `OPT_WAIT_ANY \| OPT_NO_CLEAR` | `counter == 2` | `OPT_NO_CLEAR` leaves matched bits set so multiple concurrent waiters can each succeed on the same `Set()` |
| 5 | `Timeout` | 0–2 | `OPT_WAIT_ANY` | `counter == 2` | `Wait()` returns `ERROR_TIMEOUT` within correct timing bounds `[45, 60]` ms; object remains usable after timeout |
| 6 | `TryWait` | 0–2 | `OPT_WAIT_ANY` | `counter == 3` | `TryWait()` returns an error on a non-set flag immediately; returns a match and auto-clears on a set flag; subsequent call returns error |
| 7 | `Get` | 0–2 | `OPT_WAIT_ANY` | `empty/set/clear_ok == true`, `counter == 1` | `Get()` is non-destructive at every stage; bits observed by `Get()` are still consumable by a subsequent `Wait()` |
| 8 | `MultiWaiterAny` | 0–4 | `OPT_WAIT_ANY` | `counter == 4` | Concurrent `OPT_WAIT_ANY` waiters with disjoint flag masks are all released by one `Set()` without interfering |
| 9 | `MultiWaiterAll` | 0–4 | `OPT_WAIT_ALL` | `counter == 4` | Multiple concurrent `OPT_WAIT_ALL` waiters each receive the full mask; producer re-stocks flags after each auto-clear |
| 10 | `InitialFlags` | 0–2 | `OPT_WAIT_ANY` | `counter == 2` | Constructor `initial_flags` pre-sets bits; first `Wait()` fast-paths immediately; auto-clear blocks the second call |
| 11 | `CancelWait_Infinite` | 0–2 | `OPT_WAIT_ANY` | `ERROR_CANCELED`, elapsed `[35, 75]` ms | Cancel ends an infinite wait with `ERROR_CANCELED` |
| 12 | `CancelWait_NotTimeout` | 0–2 | `OPT_WAIT_ANY` | `ERROR_CANCELED`, elapsed `< 150` ms | A cancel is not reported as a timeout and ends a timed wait early |
| 13 | `CancelWait_NoStaleWaiter` | 0–2 | `OPT_WAIT_ANY` | `Set`/`TryWait` behave normally, victim sleep `>= 55` ms | Cancel leaves no stale wait entry that could wake the task early |
| 14 | `CancelWait_FlagsUntouched` | 0–2 | `OPT_WAIT_ANY`, `OPT_NO_CLEAR` | flags word stays `FLAG_B`, then `Set`/peek/take as expected | Cancel does not modify the flags word; later operations are unaffected |
| 15 | `CancelWait_TwoWaiters` | 0–2 | `OPT_WAIT_ANY` | first `ERROR_CANCELED`, second `FLAG_A`, no leftover | Cancelling one waiter leaves the other waiter intact |
| 16 | `CancelWait_SignalWins` | 0–2 | `OPT_WAIT_ANY` | victim gets `FLAG_A`, no leftover | A `Set()` that arrived first wins over a late cancel |
| 17 | `CancelWait_WaitAllPartial` | 0–2 | `OPT_WAIT_ALL` | still blocked after partial `Set()`, then `ERROR_CANCELED`, flags `FLAG_A` | A `WAIT_ALL` waiter is cancelable after a partial-match wakeup and keeps the partial bits |
| 18 | `CancelWait_SleepCancel` | 0–2 | `OPT_WAIT_ANY` | as test 11 | Test 11 via `stk::SleepCancel()` |
| 19 | `CancelWait_SleepCancelNoStale` | 0–2 | `OPT_WAIT_ANY` | as test 13 | Test 13 via `stk::SleepCancel()` |
| 20 | `CancelWait_SleepCancelTwo` | 0–2 | `OPT_WAIT_ANY` | as test 15 | Test 15 via `stk::SleepCancel()` |
| 21 | `CancelWait_SleepCancelSignal` | 0–2 | `OPT_WAIT_ANY` | as test 16 | Test 16 via `stk::SleepCancel()` |
| 22 | `CancelWait_SleepCancelAllPartial` | 0–2 | `OPT_WAIT_ALL` | as test 17 | Test 17 via `stk::SleepCancel()` |
