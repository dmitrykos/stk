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

/*! \file  stk_c.h
    \brief C interface for C++ STK

    This file provides functions for using STK in a C project.

    \defgroup c_interface C interface for C++ STK
    \brief    Functions for using STK in C projects.

    @{
*/

#ifdef __cplusplus
extern "C" {
#endif

/*! \def   STK_KERNEL_MAX_TASKS
    \brief Define your own STK_KERNEL_MAX_TASKS if more than 8 tasks needed per kernel instance (default: 4 tasks)
*/
#ifndef STK_KERNEL_MAX_TASKS
    #define STK_KERNEL_MAX_TASKS 4
#endif

/*! \def   STK_CPU_COUNT
    \brief Define your own STK_CPU_COUNT if you need one kernel instance per CPU core (default: 1 CPU core)
*/
#ifndef STK_CPU_COUNT
    #define STK_CPU_COUNT 1
#endif

/*! \def   STK_C_ASSERT
    \brief Wrapper for assert()
    \note  Do not use assert() directly, use this definition instead
*/
#define STK_C_ASSERT(e) assert(e)

/*! \brief Opaque handle to a kernel instance */
typedef struct stk_kernel_t stk_kernel_t;

/*! \brief Opaque handle to a task instance */
typedef struct stk_task_t stk_task_t;

/*! \brief Default tick periodicity (1 ms = 1000 µs) */
#define STK_PERIODICITY_DEFAULT (1000U) /*!< microseconds */

/*! \brief Task/thread entry function prototype
 *
 *  The function is called once when the task starts.
 *  In Static mode it must never return.
 *  In Dynamic mode it may return to terminate the task.
 */
typedef void (*stk_task_entry_t)(void *arg);

/*==========================================================================
  Kernel creation
 ==========================================================================*/

/*! \brief  Create a Static (compile-time fixed) non-HRT kernel with SwitchStrategyRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails)
 */
stk_kernel_t *stk_kernel_create_static(void);

/*! \brief  Create a Dynamic (tasks can be added/removed at runtime) non-HRT kernel with SwitchStrategyRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_dynamic(void);

/*! \brief  Create a Static (compile-time fixed) non-HRT kernel with SwitchStrategySmoothWeightedRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails)
 */
stk_kernel_t *stk_kernel_create_static_swrr(void);

/*! \brief  Create a Dynamic (tasks can be added/removed at runtime) non-HRT kernel with SwitchStrategySmoothWeightedRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_dynamic_swrr(void);

/*! \brief  Create a Static (compile-time fixed) non-HRT kernel with SwitchStrategyFixedPriority scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails)
 */
stk_kernel_t *stk_kernel_create_static_fp(void);

/*! \brief  Create a Dynamic (tasks can be added/removed at runtime) non-HRT kernel with SwitchStrategyFixedPriority scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_dynamic_fp(void);

/*! \brief  Create a Static Hard Real-Time (HRT) kernel with SwitchStrategyRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_static(void);

/*! \brief  Create a Dynamic Hard Real-Time (HRT) kernel with SwitchStrategyRoundRobin scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_dynamic(void);

/*! \brief  Create a Static Hard Real-Time (HRT) kernel with SwitchStrategyRM scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_static_rm(void);

/*! \brief  Create a Dynamic Hard Real-Time (HRT) kernel with SwitchStrategyRM scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_dynamic_rm(void);

/*! \brief  Create a Static Hard Real-Time (HRT) kernel with SwitchStrategyDM scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_static_dm(void);

/*! \brief  Create a Dynamic Hard Real-Time (HRT) kernel with SwitchStrategyDM scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_dynamic_dm(void);

/*! \brief  Create a Static Hard Real-Time (HRT) kernel with SwitchStrategyEDF scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_static_edf(void);

/*! \brief  Create a Dynamic Hard Real-Time (HRT) kernel with SwitchStrategyEDF scheduling strategy
 *  \return Pointer to kernel instance (statically allocated, never fails if number of instances does not exceed STK_CPU_COUNT)
 */
stk_kernel_t *stk_kernel_create_hrt_dynamic_edf(void);

/*==========================================================================
  Kernel control
 ==========================================================================*/

/*! \brief Initialize the kernel with the desired tick period
 *  \param k              Kernel handle
 *  \param tick_period_us Tick period in microseconds (e.g. 1000 = 1 ms)
 *  \note  Must be called once before adding tasks or starting the scheduler
 */
void stk_kernel_init(stk_kernel_t *k, uint32_t tick_period_us);

/*! \brief Add a task to the scheduler (non-HRT mode)
 *  \param k     Kernel handle
 *  \param task  Task created with stk_task_create_*
 *  \note  In static kernels this must be called before stk_kernel_start()
 */
void stk_kernel_add_task(stk_kernel_t *k, stk_task_t *task);

/*! \brief Add a task with Hard Real-Time parameters (HRT kernels only)
 *  \param k                 Kernel handle
 *  \param task              Task created with stk_task_create_*
 *  \param periodicity_ticks Task period in ticks
 *  \param deadline_ticks    Maximum allowed execution time per period (ticks)
 *  \param start_delay_ticks Initial delay before first activation (ticks, ≥0)
 */
void stk_kernel_add_task_hrt(stk_kernel_t *k,
                             stk_task_t *task,
                             int32_t periodicity_ticks,
                             int32_t deadline_ticks,
                             int32_t start_delay_ticks);

/*! \brief Remove a task from scheduling (Dynamic kernels only)
 *  \param k     Kernel handle
 *  \param task  Task to remove
 *  \note  The task must have exited (returned from its entry function)
 */
void stk_kernel_remove_task(stk_kernel_t *k, stk_task_t *task);

/*! \brief Start the scheduler
 *  \param k Kernel handle
 *  \note  Never returns (transfers control to the tasks)
 */
void stk_kernel_start(stk_kernel_t *k);

/*! \brief  Check if the scheduler is running
 *  \param  k Kernel handle
 *  \return true if scheduler has been started, false otherwise
 */
bool stk_kernel_is_running(const stk_kernel_t *k);

/*! \brief  Check if a task set is schedulable
 *  \param  k Kernel handle
 *  \return true if a task set is schedulable, false otherwise
 */
bool stk_kernel_is_schedulable(const stk_kernel_t *k);

/*==========================================================================
  Task creation
 ==========================================================================*/

/*! \brief  Create a privileged (kernel-mode) task
 *  \param  entry       Task entry function
 *  \param  arg         Argument passed to the entry function
 *  \param  stack       Pointer the array of size_t elements, i.e. 32/64-bit words depending on platform)
 *  \param  stack_size  Number of elements in the stack array
 *  \return Task handle (statically allocated in static kernels, heap-allocated in dynamic)
 */
stk_task_t *stk_task_create_privileged(stk_task_entry_t entry,
                                       void *arg,
                                       size_t *stack,
                                       uint32_t stack_size);

/*! \brief  Create a user-mode task
 *  \param  entry       Task entry function
 *  \param  arg         Argument passed to the entry function
 *  \param  stack       Pointer the array of size_t elements, i.e. 32/64-bit words depending on platform)
 *  \param  stack_size  Number of elements in the stack array
 *  \return Task handle
 */
stk_task_t *stk_task_create_user(stk_task_entry_t entry,
                                 void *arg,
                                 size_t *stack,
                                 uint32_t stack_size);

/*! \brief Set weight for a task when using scheduler (non-HRT mode) with SwitchStrategySmoothWeightedRoundRobin scheduling strategy
 *  \param task   Task created with stk_task_create_*
 *  \param weight Weight of the task (must be non-zero, positive 24-bit number)
 *  \note  Weight must be set before a task is added to the kernel
 */
void stk_task_set_weight(stk_task_t *task, uint32_t weight);

/*! \brief Set priority for a task when using scheduler (non-HRT mode) with SwitchStrategyFixedPriority scheduling strategy
 *  \param task     Task created with stk_task_create_*
 *  \param priority Priority value of the task (0 is lowest, 31 is highest)
 *  \note  Priority must be set before a task is added to the kernel
 */
void stk_task_set_priority(stk_task_t *task, uint8_t priority);

/*! \brief Set task id
 *  \param task Task created with stk_task_create_*
 *  \param tid  Task id
 *  \note  For debugging purposes, can be omitted and return 0 if not used
 */
void stk_task_set_id(stk_task_t *task, uint32_t tid);

/*! \brief Set task name
 *  \param task Task created with stk_task_create_*
 *  \param tid  Task name
 *  \note  For debugging purposes, can be omitted and return NULL if not used
 */
void stk_task_set_name(stk_task_t *task, const char *tname);

/*==========================================================================
  Runtime services (available inside tasks)
 ==========================================================================*/

/*! \brief  Get thread Id.
*   \return Thread Id.
*/
size_t stk_tid(void);

/*! \brief  Get number of ticks elapsed since since kernel start
 *  \return Ticks
 */
int64_t stk_ticks(void);

/*! \brief  Get number of microseconds in one tick.
 *  \note   Tick is a periodicity of the system timer expressed in microseconds.
 *  \return Microseconds in one tick.
*/
int32_t stk_tick_resolution(void);

/*! \brief  Get current time in milliseconds since kernel start
 *  \return Time in milliseconds
 */
int64_t stk_time_now_ms(void);

/*! \brief Busy-wait delay
 *  \param ms Number of milliseconds to wait
 *  \note  Other tasks continue to run during the delay
 */
void stk_delay_ms(uint32_t ms);

/*! \brief Put current task to sleep (non-HRT mode only)
 *  \param ms Number of milliseconds to sleep
 *  \note  The CPU may enter low-power mode if all tasks are sleeping
 */
void stk_sleep_ms(uint32_t ms);

/*! \brief Yield the CPU to the next ready task
 *  \note  Useful for cooperative scheduling
 */
void stk_yield(void);

/*==========================================================================
  Cleanup (dynamic kernels only)
 ==========================================================================*/

/*! \brief Destroy a dynamically created kernel (Dynamic mode only)
 *  \param k Kernel handle (must not be running, i.e. all tasks are exited)
 */
void stk_kernel_destroy(stk_kernel_t *k);

/*! \brief Destroy a task object created with stk_task_create_* (Dynamic mode only)
 *  \param task Task handle
 */
void stk_task_destroy(stk_task_t *task);

/*==========================================================================
  Thread-Local Storage (TLS) API
 ==========================================================================*/

/*! \brief  Get raw TLS pointer (platform-specific slot)
 *  \return void * - pointer stored in a thread-local storage
 */
void *stk_tls_get(void);

/*! \brief Set raw TLS pointer
 *  \param ptr - value to store in TLS
 */
void stk_tls_set(void *ptr);

/*! \brief Helper macro to get typed TLS pointer (recommended)
 *
 * Example:
 *   typedef struct { int id; } my_tls_t;
 *   my_tls_t *tls = STK_TLS_GET(my_tls_t);
 */
#define STK_TLS_GET(type) ((type *)stk_tls_get())

/*! \brief Helper macro to set typed TLS pointer
 *
 * Example:
 *   static my_tls_t tls_data = { .id = 123 };
 *   STK_TLS_SET(&tls_data);
 */
#define STK_TLS_SET(ptr) stk_tls_set((void *)(ptr))

/*==========================================================================
  Critical Section API
 ==========================================================================*/

/*! \brief Enter critical section (disable context switching on a caller's CPU core)
 *  \note  Can be nested.
 */
void stk_enter_critical_section(void);

/*! \brief Exit critical section (re-enable context switching on a caller's CPU core)
 *  \note  Must match stk_enter_critical_section()
 */
void stk_exit_critical_section(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* STK_C_H_ */
