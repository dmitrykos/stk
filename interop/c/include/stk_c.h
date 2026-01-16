/*
 * SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_C_H_
#define STK_C_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

/*! \file   stk_c.h
    \brief  C language binding/interface for SuperTinyKernel (STK).

    This header provides a pure C API to create, configure and run STK kernel
    from C code.

    \defgroup c_api STK C API
    \brief    Pure C interface for C++ API of SuperTinyKernel (STK).
    @{
*/

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Configuration macros (can be overridden before including this file)
// ─────────────────────────────────────────────────────────────────────────────

/*! \def   STK_KERNEL_MAX_TASKS
    \brief Maximum number of tasks per kernel instance (default: 4).
    \note  Increase this value if you need more tasks.
           Has direct impact on RAM and FLASH usage.
*/
#ifndef STK_KERNEL_MAX_TASKS
    #define STK_KERNEL_MAX_TASKS 4
#endif

/*! \def   STK_CPU_COUNT
    \brief Number of kernel instances / CPU cores supported (default: 1)
    \note  Each core usually gets its own independent kernel instance.
*/
#ifndef STK_CPU_COUNT
    #define STK_CPU_COUNT 1
#endif

/*! \def STK_C_ASSERT
    \brief Assertion macro used inside STK C bindings
*/
#define STK_C_ASSERT(e) assert(e)

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief Opaque handle to a kernel instance.
*/
typedef struct stk_kernel_t stk_kernel_t;

/*! \brief Opaque handle to a task instance.
*/
typedef struct stk_task_t   stk_task_t;

/*! \brief Default tick period (1 ms).
*/
#define STK_PERIODICITY_DEFAULT  (1000U) /*!< in microseconds */

/*! \brief     Task entry point function type
    \param[in] arg: User-supplied argument (may be NULL)
    \note      If \a KERNEL_STATIC, the function must never return.
               If \a KERNEL_DYNAMIC, it may return and then task will be considered as finished.
*/
typedef void (*stk_task_entry_t)(void *arg);

// ─────────────────────────────────────────────────────────────────────────────
// Kernel factory functions
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief Create static kernel – Round Robin.
    \note  Kernel uses \a KERNEL_STATIC mode.
*/
stk_kernel_t *stk_kernel_create_static(void);

/*! \brief Create dynamic kernel – Round Robin.
    \note  Kernel uses \a KERNEL_DYNAMIC mode.
*/
stk_kernel_t *stk_kernel_create_dynamic(void);

/*! \brief Create static kernel – Smooth Weighted Round Robin.
    \note  Kernel uses \a KERNEL_STATIC mode.
*/
stk_kernel_t *stk_kernel_create_static_swrr(void);

/*! \brief Create dynamic kernel – Smooth Weighted Round Robin.
    \note  Kernel uses \a KERNEL_DYNAMIC mode.
*/
stk_kernel_t *stk_kernel_create_dynamic_swrr(void);

/*! \brief Create static kernel – Fixed Priority.
    \note  Kernel uses \a KERNEL_STATIC mode.
*/
stk_kernel_t *stk_kernel_create_static_fp(void);

/*! \brief Create dynamic kernel – Fixed Priority.
    \note  Kernel uses \a KERNEL_DYNAMIC mode.
*/
stk_kernel_t *stk_kernel_create_dynamic_fp(void);

// ───── Hard Real-Time (HRT) variants ────────────────────────────────────────

/*! \brief Create static HRT kernel – Round Robin.
    \note  Kernel uses \a KERNEL_STATIC + \a KERNEL_HRT mode.
*/
stk_kernel_t *stk_kernel_create_hrt_static(void);

/*! \brief Create dynamic HRT kernel – Round Robin.
    \note  Kernel uses \a KERNEL_DYNAMIC + \a KERNEL_HRT mode.
*/
stk_kernel_t *stk_kernel_create_hrt_dynamic(void);

/*! \brief Create static HRT kernel – Rate Monotonic.
    \note  Kernel uses \a KERNEL_STATIC + \a KERNEL_HRT mode (RM).
*/
stk_kernel_t *stk_kernel_create_hrt_static_rm(void);

/*! \brief Create dynamic HRT kernel – Rate Monotonic.
    \note  Kernel uses \a KERNEL_DYNAMIC + \a KERNEL_HRT mode (RM).
*/
stk_kernel_t *stk_kernel_create_hrt_dynamic_rm(void);

/*! \brief Create static HRT kernel – Deadline Monotonic.
    \note  Kernel uses \a KERNEL_STATIC + \a KERNEL_HRT mode (DM).
*/
stk_kernel_t *stk_kernel_create_hrt_static_dm(void);

/*! \brief Create dynamic HRT kernel – Deadline Monotonic.
    \note  Kernel uses \a KERNEL_DYNAMIC + \a KERNEL_HRT mode (DM).
*/
stk_kernel_t *stk_kernel_create_hrt_dynamic_dm(void);

/*! \brief Create static HRT kernel – Earliest Deadline First.
    \note  Kernel uses \a KERNEL_STATIC + \a KERNEL_HRT mode (EDF).
*/
stk_kernel_t *stk_kernel_create_hrt_static_edf(void);

/*! \brief Create dynamic HRT kernel – Earliest Deadline First.
    \note  Kernel uses \a KERNEL_DYNAMIC + \a KERNEL_HRT mode (EDF).
*/
stk_kernel_t *stk_kernel_create_hrt_dynamic_edf(void);

// ─────────────────────────────────────────────────────────────────────────────
// Kernel control
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief                     Initialize kernel with given tick period.
    \param[in] k:              Kernel handle.
    \param[in] tick_period_us: System tick period in microseconds (usually 100–10000).
    \note                      Must be called exactly once before adding tasks or starting scheduler.
*/
void stk_kernel_init(stk_kernel_t *k, uint32_t tick_period_us);

/*! \brief     Add task to non-HRT kernel (static or dynamic).
    \param[in] k:    Kernel handle.
    \param[in] task: Task handle created with one of stk_task_create_* functions.
    \note      For static kernels this must be done before stk_kernel_start().
*/
void stk_kernel_add_task(stk_kernel_t *k, stk_task_t *task);

/*! \brief     Add task with HRT timing parameters (HRT kernels only).
    \param[in] k:                 Kernel handle.
    \param[in] task:              Task handle.
    \param[in] periodicity_ticks: Period in ticks.
    \param[in] deadline_ticks:    Relative deadline in ticks.
    \param[in] start_delay_ticks: Initial offset / phase in ticks (>= 0).
    \note      Must be called after stk_kernel_init() and before stk_kernel_start().
*/
void stk_kernel_add_task_hrt(stk_kernel_t *k,
                             stk_task_t *task,
                             int32_t periodicity_ticks,
                             int32_t deadline_ticks,
                             int32_t start_delay_ticks);

/*! \brief     Remove finished task from dynamic kernel.
    \param[in] k:    Kernel handle.
    \param[in] task: Task that has already returned from its entry function.
    \note      Only valid in dynamic kernels. Task must have exited (returned from entry function).
*/
void stk_kernel_remove_task(stk_kernel_t *k, stk_task_t *task);

/*! \brief     Start the scheduler - never returns.
    \param[in] k: Kernel handle.
    \note      Transfers control to the scheduler and a first ready task. May return if all tasks are
               finished and kernel is dynamic.
*/
void stk_kernel_start(stk_kernel_t *k);

/*! \brief     Check whether scheduler is currently running.
    \param[in] k: Kernel handle.
    \return    True if stk_kernel_start() has been called, False otherwise.
*/
bool stk_kernel_is_running(const stk_kernel_t *k);

/*! \brief     Test whether currently configured task set is schedulable.
    \param[in] k: Kernel handle.
    \return    True if task set passes schedulability test, False otherwise.
    \note      Only meaningful for HRT RM/DM kernels.
*/
bool stk_kernel_is_schedulable(const stk_kernel_t *k);

// ─────────────────────────────────────────────────────────────────────────────
// Task creation
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief     Create privileged-mode (kernel-mode) task.
    \param[in] entry:      Task entry function.
    \param[in] arg:        Argument passed to entry function.
    \param[in] stack:      Pointer to stack buffer (array of size_t).
    \param[in] stack_size: Number of elements (words) in the stack buffer.
    \return    Task handle (static storage in static kernels, heap in dynamic).
*/
stk_task_t *stk_task_create_privileged(stk_task_entry_t entry,
                                       void *arg,
                                       size_t *stack,
                                       uint32_t stack_size);

/*! \brief     Create user-mode task.
    \param[in] entry:      Task entry function.
    \param[in] arg:        Argument passed to entry function.
    \param[in] stack:      Pointer to stack buffer (array of size_t).
    \param[in] stack_size: Number of elements (words) in the stack buffer.
    \return    Task handle.
*/
stk_task_t *stk_task_create_user(stk_task_entry_t entry,
                                 void *arg,
                                 size_t *stack,
                                 uint32_t stack_size);

/*! \brief     Set task weight (used only by Smooth Weighted Round Robin).
    \param[in] task:   Task handle.
    \param[in] weight: Positive weight value (recommended 1–16777215).
    \note      Must be called before adding task to kernel.
    \see       SwitchStrategySmoothWeightedRoundRobin.
*/
void stk_task_set_weight(stk_task_t *task, uint32_t weight);

/*! \brief     Set task priority (used only by Fixed Priority scheduler).
    \param[in] task:     Task handle.
    \param[in] priority: Priority level [0 = lowest … 31 = highest].
    \note      Must be called before adding task to kernel.
*/
void stk_task_set_priority(stk_task_t *task, uint8_t priority);

/*! \brief     Assign application-defined task ID (for tracing/debugging).
    \param[in] task: Task handle.
    \param[in] tid:  Arbitrary 32-bit task identifier.
*/
void stk_task_set_id(stk_task_t *task, uint32_t tid);

/*! \brief     Assign human-readable task name (for tracing/debugging).
    \param[in] task:  Task handle.
    \param[in] tname: Null-terminated string (may be NULL).
*/
void stk_task_set_name(stk_task_t *task, const char *tname);

// ─────────────────────────────────────────────────────────────────────────────
// Services available from inside tasks
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief  Returns current task/thread ID (the value set by stk_task_set_id).
    \return Task identifier (0 if not set).
*/
size_t stk_tid(void);

/*! \brief  Returns number of ticks elapsed since kernel start.
    \return Tick count (monotonically increasing).
*/
int64_t stk_ticks(void);

/*! \brief  Returns how many microseconds correspond to one kernel tick.
    \return Tick resolution in microseconds.
*/
int32_t stk_tick_resolution(void);

/*! \brief  Returns current time in milliseconds since kernel start.
    \return Time in milliseconds.
*/
int64_t stk_time_now_ms(void);

/*! \brief     Busy-wait delay (other tasks continue to run).
    \param[in] ms: Milliseconds to delay.
*/
void stk_delay_ms(uint32_t ms);

/*! \brief     Put current task to sleep (non-HRT kernels only).
    \param[in] ms: Milliseconds to sleep.
    \note      CPU may enter low-power mode if all tasks are sleeping.
*/
void stk_sleep_ms(uint32_t ms);

/*! \brief Voluntarily give up CPU to another ready task (cooperative yield).
*/
void stk_yield(void);

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic cleanup
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief     Destroy dynamic kernel instance (only when not running).
    \param[in] k: Kernel handle.
    \note      Kernel must not be running (all tasks must have exited or been removed).
               Only valid for kernels created with dynamic factory functions.
*/
void stk_kernel_destroy(stk_kernel_t *k);

/*! \brief     Destroy dynamically created task object.
    \param[in] task: Task handle.
    \note      Only valid for tasks created with dynamic creation functions.
               Task must no longer be scheduled (must have exited or been removed).
*/
void stk_task_destroy(stk_task_t *task);

// ─────────────────────────────────────────────────────────────────────────────
// Thread-Local Storage (very simple / one pointer per task)
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief  Get thread-local pointer (platform-specific slot).
    \return Pointer previously stored with stk_tls_set() (NULL if never set).
*/
void *stk_tls_get(void);

/*! \brief     Set thread-local pointer.
    \param[in] ptr: Pointer value to store for the current task/thread.
*/
void stk_tls_set(void *ptr);

/*! \brief Typed helper for getting TLS value.
    \note  Expands to ((type *)stk_tls_get())
*/
#define STK_TLS_GET(type) ((type *)stk_tls_get())

/*! \brief Typed helper for setting TLS value.
    \note  Expands to stk_tls_set((void *)(ptr))
*/
#define STK_TLS_SET(ptr) stk_tls_set((void *)(ptr))

/*! \example
    \code
    typedef struct {
        int task_counter;
        void *user_context;
    } my_task_local_t;

    // In task code:
    static my_task_local_t my_data = { .task_counter = 0 };
    STK_TLS_SET(&my_data);

    // Later in the same task:
    my_task_local_t *tls = STK_TLS_GET(my_task_local_t);
    tls->task_counter++;
    \endcode
*/

// ─────────────────────────────────────────────────────────────────────────────
// Critical section (interrupt / preemption protection)
// ─────────────────────────────────────────────────────────────────────────────

/*! \brief Enter critical section — disable context switches on current core.
    \note  Supports nesting (number of enter calls must match number of exit calls).
*/
void stk_enter_critical_section(void);

/*! \brief Leave critical section — re-enable context switches.
    \note  Must be called once for each previous stk_enter_critical_section().
*/
void stk_exit_critical_section(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* STK_C_H_ */
