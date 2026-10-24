# HRT Test Suite

Test suite for the Hard Real-Time (HRT) mode of the kernel (`stk::KERNEL_HRT`), including detection and
reporting of deadline misses through `ITask::OnDeadlineMissed()` and `IPlatform::IEventOverrider::OnHardFault()`.  
Source: `test/hrt/test_hrt.cpp`

---

## API Summary

```cpp
#include <stk.h>
```

```cpp
// Registration of an HRT task: periodicity, deadline and start delay are given in ticks
void IKernel::AddTask(ITask *user_task, Timeout periodicity_tc, Timeout deadline_tc, Timeout start_delay_tc);

void stk::Yield();                                      // wraps IKernelService::SwitchToNext(): in HRT mode signals job completion, sleeps until the next period
void stk::Delay(Timeout ticks);                         // busy-wait (wall-clock based), used to simulate a workload
Time stk::GetTimeNowMs();                               // milliseconds since kernel start (used to timestamp jobs)
Ticks stk::GetTicksFromMs(Time ms, uint32_t resolution);// ms -> ticks, resolution = microseconds per tick (ISR-safe form)
void ITask::OnDeadlineMissed(uint32_t duration);        // called from the tick handler when a job overruns its deadline
void IPlatform::ProcessHardFault();                     // called by the kernel after OnDeadlineMissed()
bool IPlatform::IEventOverrider::OnHardFault();         // return true: fault is handled, the system is not halted
void IPlatform::SetEventOverrider(IEventOverrider *overrider, bool non_secure = false); // before Start()
```

The HRT `AddTask()` overload is only valid before `Start()` (it asserts after `Start()` or outside
`KERNEL_HRT` mode), because the HRT schedule is built from the complete task set. Periodicity and deadline
must be `> 0` and `< INT32_MAX`; a start delay of `0` activates the task immediately.

An HRT task is a periodic job: it is activated at `start_delay + i * periodicity`, does its work and calls
`Yield()` to report completion. The kernel then excludes the task from scheduling until its next period.
The deadline is measured from the moment of activation, so it also covers the time the task waits for the CPU.

| Call | Behaviour |
|------|-----------|
| `AddTask(task, period, deadline, delay)` | Registers an HRT task. First activation occurs after `delay` ticks, then every `period` ticks. |
| `Yield()` | In HRT mode signals completion of the current job and acts as a cooperation point; `NOSLEEP_YIELD_API` of the strategy is ignored. The task sleeps until its next period (absolute grid, no drift). Task context only, ISR-unsafe; the call busy-waits until the kernel has switched the task out. |
| `Delay(N)` | Busy-waits (spins in a loop) until the deadline of `N` ticks expires; a negative value asserts. `Sleep()` / `SleepUntil()` are unsupported in HRT mode (tasks sleep automatically by periodicity and workload), so the workload is simulated with `Delay()`. Use with care in HRT mode to avoid a missed deadline. |
| `GetTimeNowMs()` | Milliseconds since `IKernel::Start()`; with the default 1 ms tick it equals the tick count. Used to timestamp the start and end of every job. |
| `GetTicksFromMs(ms, resolution)` | Converts milliseconds to ticks without querying the kernel (ISR-safe). The suite wraps it as `MsToTicks()` with `PERIODICITY_DEFAULT` (1000 us per tick). |
| Return from `Run()` | Task exits (`KERNEL_DYNAMIC`): the slot is freed on the next tick and `ITask::OnExit()` is called. Remaining tasks keep their schedule. |
| `OnDeadlineMissed(duration)` | Called when the deadline is exceeded; `duration` is the elapsed active time in ticks and is always greater than the deadline. With the strict rule the first report is `deadline + 1`. Optional handler, intended for fault logging. |
| `ProcessHardFault()` | Invoked by the kernel after `OnDeadlineMissed()` returns, unless the switch strategy absorbs the overrun (`DEADLINE_MISSED_API`; `SwitchStrategyRoundRobin` does not). Enters the safe state of the system. |
| `OnHardFault()` | Optional platform event overrider hook. Returns `true` if the event is handled (the driver does not halt the system), `false` to let the driver handle it. |

**Key invariants:**

- Job starts stay on the `base + delay + i * period` grid: no accumulated drift over many periods.
- A healthy job (workload below its deadline) never triggers `OnDeadlineMissed()` or a hard fault.
- An overrun is detected with the strict rule `duration > deadline`; the first reported duration is `deadline + 1`.
- A deadline miss raises a hard fault; with a handling overrider the system survives and keeps running.
- An overrunning task does not disturb the schedule of a healthy task.
- A task may exit at any time (after `Yield()`, by returning from `Run()`, or without a single job) without
  affecting the remaining tasks.

---

## Test Configuration

| Constant | Value | Purpose |
|----------|-------|---------|
| `_STK_HRT_TEST_TASKS_MAX` | `4` | Kernel task capacity (and number of task objects created by `RunTest`) |
| `_STK_HRT_TEST_ITRS_MAX` | `20` | Maximum number of jobs recorded per task |
| `_STK_HRT_TEST_START_TOL_MS` | `10` ms | Allowed deviation of a job start from its expected time |
| `_STK_HRT_TEST_WORK_LO_MS` | `1` ms | Allowed "early" slack of a measured workload (measurement jitter) |
| `_STK_HRT_TEST_WORK_HI_MS` | `10` ms | Allowed "late" slack of a measured workload |
| `_STK_HRT_STACK_SIZE` | `384` (RISC-V) / `256` (others) | Per-task stack size in `size_t` words |

Timing assumes the default 1 ms tick. A measured workload of `W` ms is accepted in `[W - 1, W + 10]`.
A job start is accepted within `±10` ms of `base + delay + i * period`.

The suite is built once, with the kernel configured as:

```
Kernel<KERNEL_DYNAMIC | KERNEL_HRT, 4, SwitchStrategyRoundRobin, PlatformDefault>
```

`KERNEL_DYNAMIC` makes `Kernel::Start()` return once all tasks have exited, which is how each test completes
and is verified. Task objects are `static` inside `RunTest`, so a removed task (and its stack) is reused by the
next test.

**Per-task configuration (`TaskCfg`, all times in ms):**

| Field | Purpose |
|-------|---------|
| `period` | HRT periodicity |
| `deadline` | HRT deadline |
| `delay` | Start delay |
| `work` | Workload of one job (busy delay) |
| `iterations` | Number of jobs before the task exits |
| `yield_last` | `Yield()` after the last job (`false`: return from `Run()` right away) |
| `expect_miss` | Workload exceeds the deadline: a deadline miss is expected |

**Test state:** all shared state lives in static arrays indexed by task id, and every element has a single writer:

| Variable | Purpose |
|----------|---------|
| `g_Cfg[id]` | Task configuration of the current scenario |
| `g_Start[id][job]` | Recorded start time (ms) of every job |
| `g_Diff[id][job]` | Measured workload (ms) of every job |
| `g_Count[id]` | Number of completed jobs |
| `g_MissCount[id]` | Number of `OnDeadlineMissed()` calls |
| `g_MissFirst[id]` | Duration reported by the first `OnDeadlineMissed()` call |
| `g_Overrider.m_faults` | Number of hard faults counted by the event overrider |

`ResetTestState()` clears all state before each test. `RunTest<T>(name, scenario)` fills `g_Cfg` through
`SetupScenario()`, adds the tasks with their periodicity / deadline / delay converted to ticks, runs the kernel
and verifies the recorded schedule.

**Hard fault handling.** `HrtOverrider` implements `IPlatform::IEventOverrider`; its `OnHardFault()` counts the
fault and returns `true` (handled) so the platform driver does not halt the system. It is installed once in
`main()` with `SetEventOverrider()` before `Start()`. In all scenarios without an expected miss the fault counter
must stay zero.

---

## Verification

The kernel tick counter keeps running between tests, so expected times are relative to a base derived from the
first job of the first task that ran at least one job:

```
base   = start[ref][0] - delay[ref]
expect = base + delay + i * period        (expected start of job i)
```

`VerifyScenario()` checks, for every task:

| Check | Condition |
|-------|-----------|
| Jobs done | `g_Count == iterations` |
| Start time | `abs(start - expect) <= 10` ms |
| Workload | `diff` in `[work - 1, work + 10]` ms |
| Deadline (healthy tasks) | `diff <= deadline` |
| Misses (healthy tasks) | `g_MissCount == 0` |
| Misses (`expect_miss`) | `g_MissCount > 0` and `g_MissFirst == deadline + 1` |
| Hard faults | none if no miss expected, at least one if any miss expected |

---

## Platform Notes

The task stack is larger on RISC-V (`384` words) than on other architectures (`256` words). All tests run on
every platform the suite is built for.

---

## Tests

All tests use the `HrtTask` class and differ only in the scenario passed to `RunTest`. Times below are in ms
(`period / deadline / delay / work / jobs`).

### Test 1 — `Basic` (`SC_BASIC`)
**Tasks:** 0–2

Tasks are activated one after another every 100 ms with a varying workload of 10 / 20 / 30 ms, period 300 ms,
deadline 50 ms, 3 jobs each. This is the original test of the suite.

| Task | Period | Deadline | Delay | Work | Jobs |
|------|--------|----------|-------|------|------|
| 0 | 300 | 50 | 0 | 10 | 3 |
| 1 | 300 | 50 | 100 | 20 | 3 |
| 2 | 300 | 50 | 200 | 30 | 3 |

**Pass condition:** all jobs done, starts on the grid, workloads in range, no deadline miss, no hard fault

---

### Test 2 — `SingleTask` (`SC_SINGLE_TASK`)
**Tasks:** 0

One task alone in the system (`50 / 25 / 0 / 5 / 5`). The CPU enters the sleep trap every period.

**Pass condition:** as test 1

---

### Test 3 — `ZeroWork` (`SC_ZERO_WORK`)
**Tasks:** 0

A job completes immediately: `Yield()` without any work (`20 / 10 / 0 / 0 / 6`). The whole period is spent
sleeping.

**Pass condition:** as test 1

---

### Test 4 — `Simultaneous` (`SC_SIMULTANEOUS`)
**Tasks:** 0–2

Three tasks become active at the same moment (`60 / 40 / 0`, work 10 / 15 / 20, 3 jobs). The deadline also covers
the time spent waiting for the CPU. Workloads overlap because `Delay()` is wall-clock based.

**Pass condition:** as test 1

---

### Test 5 — `TightDeadline` (`SC_TIGHT_DEADLINE`)
**Tasks:** 0–1

The deadline is only 8 ms above the workload (`100 / 28 / 0 / 20 / 4` and `100 / 28 / 50 / 20 / 4`). No miss is
allowed.

**Pass condition:** as test 1

---

### Test 6 — `LongStartDelay` (`SC_LONG_START_DELAY`)
**Tasks:** 0–1

Task 1 first starts after 200 ms, longer than its own period of 80 ms, while task 0 runs 8 jobs of period 50 ms
(`50 / 20 / 0 / 5 / 8` and `80 / 30 / 200 / 10 / 2`).

**Pass condition:** as test 1

---

### Test 7 — `DifferentPeriods` (`SC_DIFFERENT_PERIODS`)
**Tasks:** 0–2

Tasks with different periodicities and workloads run side by side: periods 40 / 60 / 100, delays 0 / 10 / 20,
work 5 / 8 / 12, deadline 30.

**Pass condition:** as test 1

---

### Test 8 — `NoDrift` (`SC_NO_DRIFT`)
**Tasks:** 0

A single task runs 20 periods (`20 / 10 / 0 / 3 / 20`). Every start must stay on the `start + i * period` grid,
so the period is exact and no drift accumulates.

**Pass condition:** as test 1

---

### Test 9 — `StaggeredExit` (`SC_STAGGERED_EXIT`)
**Tasks:** 0–2

Tasks exit after 1, 3 and 6 jobs (`30 / 20`, delays 0 / 10 / 20, work 5). The remaining tasks must keep their
schedule while others leave.

**Pass condition:** as test 1

---

### Test 10 — `ExitWithoutYield` (`SC_EXIT_WITHOUT_YIELD`)
**Tasks:** 0–1

The last job ends by returning from `Run()` instead of calling `Yield()` (`yield_last = false`). Tasks
`40 / 20 / 0 / 5 / 3` and `40 / 20 / 20 / 5 / 3`.

**Pass condition:** as test 1

---

### Test 11 — `ImmediateExit` (`SC_IMMEDIATE_EXIT`)
**Tasks:** 0–1

Task 0 returns from `Run()` without running a single job (`iterations = 0`), task 1 runs 3 jobs. The reference
task for the time base is the first task that executed at least one job.

**Pass condition:** task 0 has zero jobs, task 1 as test 1

---

### Test 12 — `FullPool` (`SC_FULL_POOL`)
**Tasks:** 0–3 (all 4)

All kernel task slots are used. Period 200, deadline 40, delays 0 / 50 / 100 / 150, work 10 / 20 / 30 / 10,
3 jobs each. Activations are spread across the common period.

**Pass condition:** as test 1

---

### Test 13 — `DeadlineMiss` (`SC_DEADLINE_MISS`)
**Tasks:** 0

Workload 40 ms against a deadline of 20 ms (`100 / 20 / 0 / 40 / 2`, `expect_miss`). The overrun is detected by the
strict `duration > deadline` rule, so the first reported duration is `deadline + 1 = 21`, and a hard fault is
raised and survived through `HrtOverrider`.

**Pass condition:** `g_MissCount > 0`, `g_MissFirst == 21`, at least one hard fault, all jobs still completed

---

### Test 14 — `MissIsolated` (`SC_MISS_ISOLATED`)
**Tasks:** 0–1

Task 0 overruns (`100 / 20 / 0 / 40 / 3`, `expect_miss`), task 1 is healthy (`100 / 30 / 50 / 10 / 3`). Verifies
that the missing task does not disturb the schedule of the healthy one.

**Pass condition:** task 0 as test 13; task 1 as test 1 (no miss reported, start times and workloads in range)

---

## Summary Table

| # | Test | Tasks | Pass condition | What it verifies |
|---|------|-------|----------------|------------------|
| 1 | `Basic` | 0–2 | all jobs, grid, workload, no miss | Staggered start delays, varying workload |
| 2 | `SingleTask` | 0 | as 1 | Single task, sleep trap entered every period |
| 3 | `ZeroWork` | 0 | as 1 | Job that completes immediately |
| 4 | `Simultaneous` | 0–2 | as 1 | Deadline covers waiting time for the CPU |
| 5 | `TightDeadline` | 0–1 | as 1 | Deadline only 8 ms above workload |
| 6 | `LongStartDelay` | 0–1 | as 1 | Start delay longer than the task's period |
| 7 | `DifferentPeriods` | 0–2 | as 1 | Different periodicities side by side |
| 8 | `NoDrift` | 0 | as 1 | 20 periods without accumulated drift |
| 9 | `StaggeredExit` | 0–2 | as 1 | Tasks exit after 1 / 3 / 6 jobs, others keep schedule |
| 10 | `ExitWithoutYield` | 0–1 | as 1 | Last job ends by returning from `Run()` |
| 11 | `ImmediateExit` | 0–1 | task 0 no jobs, task 1 as 1 | Exit without running a single job |
| 12 | `FullPool` | 0–3 | as 1 | All kernel task slots used |
| 13 | `DeadlineMiss` | 0 | miss reported, first duration `deadline + 1`, hard fault | Strict miss detection, `OnDeadlineMissed()` + `OnHardFault()` |
| 14 | `MissIsolated` | 0–1 | task 0 as 13, task 1 healthy | Miss isolated from a healthy task |
