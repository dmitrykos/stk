# Event Test Suite

Test suite for `stk::sync::Event`.  
Source: `test/event/test_event.cpp`

---

## API Summary

```cpp
#include <sync/stk_sync_event.h>
```

`Event` is a binary synchronization primitive (signaled / non-signaled). Two operation
modes are selected at construction time:

- **Auto-reset** (default): `Set()` wakes one waiting task; the event resets to
  non-signaled immediately after that task is released.
- **Manual-reset**: `Set()` wakes all waiting tasks; the signaled state persists until
  `Reset()` is called explicitly.

Requires kernel mode: `KERNEL_DYNAMIC | KERNEL_SYNC`.

```cpp
// Construction
stk::sync::Event g_Evt;                 // auto-reset, initially non-signaled (default)
stk::sync::Event g_Evt(true);           // manual-reset, initially non-signaled
stk::sync::Event g_Evt(false, true);    // auto-reset, initially signaled
stk::sync::Event g_Evt(true,  true);    // manual-reset, initially signaled
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `Set` | `bool Set()` | Transitions to signaled state. Auto-reset: wakes one waiter and auto-resets. Manual-reset: wakes all waiters, stays signaled. Returns `true` if state changed, `false` if already signaled. ISR-safe. |
| `Reset` | `bool Reset()` | Transitions to non-signaled state. Returns `true` if state changed, `false` if already non-signaled. ISR-safe. |
| `Wait` | `bool Wait(Timeout timeout = WAIT_INFINITE)` | Blocks until signaled or timeout expires. Takes fast path (returns immediately) if already signaled, auto-resetting in auto-reset mode. Returns `true` if signaled, `false` on timeout. ISR-unsafe. |
| `TryWait` | `bool TryWait()` | Non-blocking poll. Returns `true` if signaled at the time of call (auto-resets in auto-reset mode); `false` otherwise. Never blocks. ISR-safe. |
| `Pulse` | `void Pulse()` | Win32 `PulseEvent()` semantics: wakes waiters (one for auto-reset, all for manual-reset) then forces non-signaled, regardless of whether anyone was waiting. ISR-safe. |
| `~Event` | (destructor) | Asserts `m_wait_list.IsEmpty()` in debug builds. Destroying an event with active waiters is a logic error. |

**Behavior by mode:**

| Operation | Auto-reset | Manual-reset |
|-----------|------------|--------------|
| `Set()` with waiters | Wakes one; resets immediately | Wakes all; stays signaled |
| `Set()` with no waiters | Stays signaled; next `Wait()` fast-paths and resets | Stays signaled; `Wait()` fast-paths without resetting |
| `Reset()` | Clears signaled state; returns `false` if already clear | Clears signaled state; returns `false` if already clear |
| `Pulse()` with waiters | Wakes one; resets | Wakes all; resets |
| `Pulse()` with no waiters | Resets (state never persists) | Resets (state never persists) |
| `TryWait()` signaled | Returns `true`; auto-resets | Returns `true`; does **not** reset |
| `TryWait()` not signaled | Returns `false` immediately | Returns `false` immediately |

**Key invariants:**

- `Set()` and `Reset()` return values reflect whether the state **actually changed**.
- `Reset()` on an already non-signaled event is a no-op returning `false`.
- Auto-reset happens inside `RemoveWaitObject()` when the waiter is released, not
  inside `Set()` itself — the signaled state is cleared atomically with wake delivery.
- Destroying an `Event` while tasks are waiting is a logic error; an assertion fires
  in debug builds.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_EVT_TEST_TASKS_MAX` | `5` | Total tasks per test run |
| `_STK_EVT_TEST_TIMEOUT` | `300` ticks | Blocking timeout for `Wait()` calls that must succeed |
| `_STK_EVT_TEST_SHORT_SLEEP` | `10` ticks | Sleep used to pace task sequencing |
| `_STK_EVT_TEST_LONG_SLEEP` | `100` ticks | Sleep used by the verifier task to wait for workers |
| `_STK_EVT_STACK_SIZE` | `128` (M0) / `256` (others) | Per-task stack size in `size_t` words |

`g_TestEvent` is **reconstructed in-place** via placement-new inside `ResetTestState()`
before each test. `ResetTestState()` accepts `manual_reset` and `initial_state`
parameters that are forwarded directly to the `Event` constructor, so the same helper
serves all eight tests without any additional setup code per test.

---

## Platform Notes

On **Cortex-M0** (`__ARM_ARCH_6M__`) the device has insufficient RAM to link eight
distinct task class templates simultaneously. Test 1 (`AutoResetBasic`) runs on all
platforms; tests 2–8 are skipped on M0 under `#ifndef __ARM_ARCH_6M__`.

`AutoResetBasic` runs on M0 because it uses a single task class template
(`AutoResetBasicTask`) instantiated for all five task slots, fitting within the
available memory. Tests 2–8 each introduce a distinct task class template, multiplying
the static memory footprint beyond what M0 can accommodate.

| Platform | `_STK_EVT_STACK_SIZE` |
|----------|-----------------------|
| Cortex-M0 (`__ARM_ARCH_6M__`) | `128` words |
| All others | `256` words |

---

## Tests

### Test 1 — `AutoResetBasic`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Mode:** auto-reset &nbsp;|&nbsp; **Param:** `iterations = 20` &nbsp;|&nbsp; **Runs on M0: yes**

Task 0 is the producer; tasks 1–4 are consumers. All four consumers loop calling
`Wait(_STK_EVT_TEST_TIMEOUT)`. The producer calls `Set()` 20 times, pacing each with
`Delay(1)` so each signal lands on exactly one blocked consumer before the next fires.
Each successful wake increments `g_SharedCounter`. Verifies that after every `Set()`
the event auto-resets, keeping the remaining consumers blocked.

**Pass condition:** `counter == 20`

---

### Test 2 — `ManualResetBasic`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Mode:** manual-reset

All four consumers (tasks 1–4) block on `Wait(_STK_EVT_TEST_TIMEOUT)`. After
`_STK_EVT_TEST_SHORT_SLEEP` ticks the producer (task 0) fires a single `Set()`.
All four must be released simultaneously. The producer then sleeps
`_STK_EVT_TEST_LONG_SLEEP` and calls `TryWait()` to confirm the event is **still
signaled** (manual-reset does not auto-clear), before explicitly calling `Reset()`.

**Pass condition:** `counter == 4` and `still_signaled == true`

---

### Test 3 — `InitialState`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Mode:** auto-reset, `initial_state = true`

Task 1 calls `Wait(_STK_EVT_TEST_SHORT_SLEEP)` immediately. Because the event was
constructed pre-signaled, `Wait()` takes the fast path and returns `true` without
blocking, and the event auto-resets. Task 1 then calls `Wait()` a second time; the
event is now non-signaled so this must time out and return `false`. Verifies the
`initial_state` constructor parameter and the fast-path auto-reset behavior.

**Pass condition:** `counter == 2`
(1 = first `Wait()` succeeded immediately on pre-signaled event; 2 = second `Wait()` timed out after auto-reset)

---

### Test 4 — `TimeoutWait`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Mode:** auto-reset

Task 1 calls `Wait(50)` after `_STK_EVT_TEST_SHORT_SLEEP`; no producer fires within
the 50-tick window so it must return `false` with elapsed time in `[45, 60]` ms.
Task 0 withholds `Set()` until tick 200. Task 2 sleeps until tick 210 then calls
`Wait(100)`, which must succeed on the now-signaled event. Task 2 is the verifier.

**Pass condition:** `counter == 2`
(1 = timeout fired correctly within bounds; 2 = `Wait()` succeeded after `Set()`)

---

### Test 5 — `TryWait`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Mode:** auto-reset

Task 1 calls `TryWait()` on a non-signaled event; must return `false` immediately
(elapsed < `_STK_EVT_TEST_SHORT_SLEEP`). Task 2 calls `Set()` then `TryWait()`;
must return `true` immediately (event just signaled) and auto-reset. Task 2 then
calls `TryWait()` a second time to confirm the reset: must return `false`. Covers
both the non-signaled and signaled fast paths, and the auto-reset `TryWait()` performs
on success.

**Pass condition:** `counter == 3`
(1 = non-signaled `TryWait()` returned `false` immediately; 2 = signaled `TryWait()` returned `true` immediately; 3 = post-reset `TryWait()` returned `false`)

---

### Test 6 — `ResetManual`
**Tasks:** 0–2 only &nbsp;|&nbsp; **Mode:** manual-reset

Task 0 calls `Set()` then immediately `Reset()` before tasks 1 and 2 have entered
`Wait()`. Verifies three return values: `Set()` returns `true` (state changed),
first `Reset()` returns `true` (state changed), second `Reset()` returns `false`
(already non-signaled). Tasks 1 and 2 sleep briefly then call
`Wait(_STK_EVT_TEST_SHORT_SLEEP)`; both must time out because the event was cleared
before they arrived. Any unexpected wake increments `g_SharedCounter`.

**Pass condition:** `set_changed == true`, `reset_changed == true`, `reset_again == false`, `counter == 0`

---

### Test 7 — `PulseAutoReset`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Mode:** auto-reset &nbsp;|&nbsp; **Param:** `iterations = 20`

Task 0 is the producer; tasks 1–4 are consumers. Each consumer loops waiting for
`iterations / 4 = 5` pulses. The producer calls `Pulse()` 20 times with `Delay(1)`
between calls, each waking exactly one consumer. After all consumers exit, the
producer fires one more `Pulse()` with no waiters present, then calls `TryWait()` to
confirm the event is non-signaled — `Pulse()` always resets regardless of whether
anyone was waiting.

**Pass condition:** `counter == 20` and `still_signaled == false`

---

### Test 8 — `PulseManualReset`
**Tasks:** 0–4 (all 5) &nbsp;|&nbsp; **Mode:** manual-reset

All four consumers (tasks 1–4) block on `Wait(_STK_EVT_TEST_TIMEOUT)`. After
`_STK_EVT_TEST_SHORT_SLEEP` ticks the producer fires a single `Pulse()`, which must
release all four simultaneously. Unlike `Set()` on a manual-reset event, `Pulse()`
always resets to non-signaled — verified by `TryWait()` returning `false` immediately
after. A second `Pulse()` with no waiters must also leave the event non-signaled.

**Pass condition:** `counter == 4`, `still_signaled == false`, `after_empty_pulse == false`

---

## Summary Table

| # | Test | Tasks | Mode | Pass condition | What it verifies |
|---|------|-------|------|----------------|------------------|
| 1 | `AutoResetBasicTask` | 0–4 | auto-reset | `counter == 20` | `Set()` wakes exactly one waiter per call and auto-resets; runs on all platforms including M0 |
| 2 | `ManualResetBasicTask` | 0–4 | manual-reset | `counter == 4`, `still_signaled == true` | `Set()` wakes all waiters simultaneously; state stays signaled until `Reset()` is called |
| 3 | `InitialStateTask` | 0–2 | auto-reset, pre-signaled | `counter == 2` | `initial_state=true` causes `Wait()` to fast-path immediately; auto-reset clears the state for the next caller |
| 4 | `TimeoutWaitTask` | 0–2 | auto-reset | `counter == 2` | `Wait()` returns `false` within the correct timing window `[45, 60]` ms; event usable after timeout |
| 5 | `TryWaitTask` | 0–2 | auto-reset | `counter == 3` | `TryWait()` returns `false` on non-signaled; returns `true` and auto-resets on signaled; subsequent `TryWait()` returns `false` |
| 6 | `ResetManualTask` | 0–2 | manual-reset | `set/reset_changed` correct, `counter == 0` | `Reset()` return values reflect actual state change; waiters arriving after reset time out correctly |
| 7 | `PulseAutoResetTask` | 0–4 | auto-reset | `counter == 20`, `still_signaled == false` | `Pulse()` wakes exactly one waiter and always resets, including when no waiters are present |
| 8 | `PulseManualResetTask` | 0–4 | manual-reset | `counter == 4`, both booleans `false` | `Pulse()` wakes all waiters and always resets — unlike `Set()`, which leaves manual-reset events signaled |
