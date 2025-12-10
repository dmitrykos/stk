/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include <cstddef> // for std::size_t

#include <stk_config.h>
#include <stk.h>
#include "stk_c.h"

#define STK_TASKS_MAX (STK_KERNEL_MAX_TASKS * STK_CPU_COUNT)

using namespace stk;

typedef Kernel<KERNEL_STATIC, STK_KERNEL_MAX_TASKS, SwitchStrategyRR, PlatformDefault> KernelStaticRR;
typedef Kernel<KERNEL_DYNAMIC, STK_KERNEL_MAX_TASKS, SwitchStrategyRR, PlatformDefault> KernelDynamicRR;
typedef Kernel<KERNEL_STATIC, STK_KERNEL_MAX_TASKS, SwitchStrategySWRR, PlatformDefault> KernelStaticSWRR;
typedef Kernel<KERNEL_DYNAMIC, STK_KERNEL_MAX_TASKS, SwitchStrategySWRR, PlatformDefault> KernelDynamicSWRR;
typedef Kernel<KERNEL_STATIC | KERNEL_HRT, STK_KERNEL_MAX_TASKS, SwitchStrategyRR, PlatformDefault> KernelStaticHrtRR;
typedef Kernel<KERNEL_DYNAMIC | KERNEL_HRT, STK_KERNEL_MAX_TASKS, SwitchStrategyRR, PlatformDefault> KernelDynamicHrtRR;

inline void *operator new(std::size_t, void *ptr) noexcept
{
    return ptr;
}

inline void operator delete(void *, void *) noexcept
{
    // nothing to do for placement delete
}

class TaskWrapper : public ITask
{
public:
    // ITask
    RunFuncType GetFunc() { return m_func; }
    void *GetFuncUserData() { return m_user_data; }
    EAccessMode GetAccessMode() const { return m_mode; }
    virtual void OnDeadlineMissed(uint32_t duration) { (void)duration; }
    virtual int32_t GetWeight() const { return m_weight; }

    // IStackMemory
    size_t *GetStack() const { return m_stack; }
    uint32_t GetStackSize() const { return m_stack_size; }
    uint32_t GetStackSizeBytes() const { return m_stack_size * sizeof(size_t); }

    void Initialize(RunFuncType func,
                    void       *user_data,
                    size_t     *stack,
                    size_t      stack_size,
                    EAccessMode mode)
    {
        m_func       = func;
        m_user_data  = user_data;
        m_stack      = stack;
        m_stack_size = stack_size;
        m_mode       = mode;
        m_weight     = 1;
    }

    void SetWeight(int32_t weight) { m_weight = weight; }

private:
    RunFuncType m_func;
    void       *m_user_data;
    size_t     *m_stack;
    size_t      m_stack_size;
    EAccessMode m_mode;
    int32_t     m_weight;
};

struct TaskSlot
{
    TaskSlot() : busy(false), task()
    {}

    bool        busy;
    TaskWrapper task;
};

class KernelWrapper
{
public:
    enum Type
    {
        None,
        StaticRR,
        DynamicRR,
        StaticSWRR,
        DynamicSWRR,
        StaticHrtRR,
        DynamicHrtRR
    };

    Type     active;
    IKernel *ptr;

    KernelWrapper() : active(Type::None), ptr(nullptr)
    {}

    ~KernelWrapper()
    {
        Destroy();
    }

    IKernel *Create(Type req_type)
    {
        Destroy();

        active = req_type;

        switch (req_type)
        {
        case Type::StaticRR:
            ptr = new (&static_rr) KernelStaticRR();
            break;
        case Type::DynamicRR:
            ptr = new (&dynamic_rr) KernelDynamicRR();
            break;
        case Type::StaticSWRR:
            ptr = new (&static_swrr) KernelStaticSWRR();
            break;
        case Type::DynamicSWRR:
            ptr = new (&dynamic_swrr) KernelDynamicSWRR();
            break;
        case Type::StaticHrtRR:
            ptr = new (&static_hrt_rr) KernelStaticHrtRR();
            break;
        case Type::DynamicHrtRR:
            ptr = new (&dynamic_hrt_rr) KernelDynamicHrtRR();
            break;
        default:
            STK_ASSERT(false);
            break;
        }

        return ptr;
    }

    void Destroy()
    {
        switch (active)
        {
        case Type::StaticRR:
            static_rr.~KernelStaticRR();
            break;
        case Type::DynamicRR:
            dynamic_rr.~KernelDynamicRR();
            break;
        case Type::StaticSWRR:
            static_swrr.~KernelStaticSWRR();
            break;
        case Type::DynamicSWRR:
            dynamic_swrr.~KernelDynamicSWRR();
            break;
        case Type::StaticHrtRR:
            static_hrt_rr.~KernelStaticHrtRR();
            break;
        case Type::DynamicHrtRR:
            dynamic_hrt_rr.~KernelDynamicHrtRR();
            break;
        case Type::None:
            break;
        }

        active = Type::None;
        ptr = nullptr;
    }

private:

    union
    {
        KernelStaticRR static_rr;
        KernelDynamicRR dynamic_rr;
        KernelStaticSWRR static_swrr;
        KernelDynamicSWRR dynamic_swrr;
        KernelStaticHrtRR static_hrt_rr;
        KernelDynamicHrtRR dynamic_hrt_rr;
    };
};

// Static vars
static KernelWrapper s_Kernel[STK_CPU_COUNT];
static TaskSlot      s_Tasks[STK_TASKS_MAX];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static IKernel *AllocateKernel(KernelWrapper::Type type)
{
    for (uint32_t i = 0; i < STK_CPU_COUNT; ++i)
    {
        if (KernelWrapper::Type::None == s_Kernel[i].active)
            return s_Kernel[i].Create(type);
    }

    STK_ASSERT(false);
    return nullptr;
}

static void DestroyKernel(const IKernel *kernel)
{
    for (uint32_t i = 0; i < STK_CPU_COUNT; ++i)
    {
        if ((KernelWrapper::Type::None != s_Kernel[i].active) &&
            (kernel == s_Kernel[i].ptr))
        {
            s_Kernel[i].Destroy();
            return;
        }
    }

    STK_ASSERT(false);
}

static TaskWrapper *AllocateTask(stk_task_entry_t entry,
                                 void *arg,
                                 uint32_t *stack,
                                 uint32_t stack_size,
                                 EAccessMode mode)
{
    for (uint32_t i = 0; i < STK_TASKS_MAX; ++i)
    {
        if (!s_Tasks[i].busy)
        {
            s_Tasks[i].busy = true;

            TaskWrapper *task = &s_Tasks[i].task;
            task->Initialize(entry, arg, stack, stack_size, mode);
            return task;
        }
    }

    STK_ASSERT(false);
    return nullptr;
}

static void FreeTask(const TaskWrapper *task)
{
    for (uint32_t i = 0; i < STK_TASKS_MAX; ++i)
    {
        if (s_Tasks[i].busy && (task == &s_Tasks[i].task))
        {
            s_Tasks[i].busy = false;
            return;
        }
    }

    STK_ASSERT(false);
}

// ---------------------------------------------------------------------------
// C-interface
// ---------------------------------------------------------------------------
extern "C" {

// ---------------------------------------------------------------------------
// Kernel create/destroy wrappers
// ---------------------------------------------------------------------------
stk_kernel_t *stk_kernel_create_static()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::StaticRR));
}

stk_kernel_t *stk_kernel_create_dynamic()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::DynamicRR));
}

stk_kernel_t *stk_kernel_create_static_swrr()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::StaticSWRR));
}

stk_kernel_t *stk_kernel_create_dynamic_swrr()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::DynamicSWRR));
}

stk_kernel_t *stk_kernel_create_hrt_static()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::StaticHrtRR));
}

stk_kernel_t *stk_kernel_create_hrt_dynamic()
{
    return reinterpret_cast<stk_kernel_t *>(AllocateKernel(KernelWrapper::DynamicHrtRR));
}

void stk_kernel_destroy(stk_kernel_t *k)
{
    STK_ASSERT(k);
    DestroyKernel(reinterpret_cast<IKernel *>(k));
}

// ---------------------------------------------------------------------------
// Kernel control wrappers
// ---------------------------------------------------------------------------
void stk_kernel_init(stk_kernel_t *k,
                     uint32_t tick_period_us)
{
    STK_ASSERT(k);
    reinterpret_cast<stk::IKernel *>(k)->Initialize(tick_period_us);
}

void stk_kernel_start(stk_kernel_t *k)
{
    STK_ASSERT(k);
    reinterpret_cast<stk::IKernel *>(k)->Start();
}

bool stk_kernel_is_running(const stk_kernel_t *k)
{
    STK_ASSERT(k);
    return reinterpret_cast<const stk::IKernel *>(k)->IsStarted();
}

void stk_kernel_add_task(stk_kernel_t *k, stk_task_t *task)
{
    STK_ASSERT(k);
    STK_ASSERT(task);
    reinterpret_cast<stk::IKernel *>(k)->AddTask(
        reinterpret_cast<TaskWrapper *>(task));
}

void stk_kernel_remove_task(stk_kernel_t *k, stk_task_t *task)
{
    STK_ASSERT(k);
    STK_ASSERT(task);
    reinterpret_cast<stk::IKernel *>(k)->RemoveTask(
        reinterpret_cast<TaskWrapper *>(task));
}

void stk_kernel_add_task_hrt(stk_kernel_t *k,
                             stk_task_t *task,
                             int32_t periodicity_ticks,
                             int32_t deadline_ticks,
                             int32_t start_delay_ticks)
{
    STK_ASSERT(k);
    STK_ASSERT(task);
    reinterpret_cast<stk::IKernel *>(k)->AddTask(
        reinterpret_cast<TaskWrapper *>(task),
        periodicity_ticks,
        deadline_ticks,
        start_delay_ticks);
}

// ---------------------------------------------------------------------------
// Task creation
// ---------------------------------------------------------------------------
stk_task_t *stk_task_create_privileged(stk_task_entry_t entry,
                                       void *arg,
                                       uint32_t *stack,
                                       uint32_t stack_size)
{
    STK_ASSERT(entry);
    STK_ASSERT(stack);
    STK_ASSERT(stack_size);
    return reinterpret_cast<stk_task_t *>(AllocateTask(entry, arg, stack, stack_size, ACCESS_PRIVILEGED));
}

stk_task_t *stk_task_create_user(stk_task_entry_t entry,
                                 void *arg,
                                 uint32_t *stack,
                                 uint32_t stack_size)
{
    STK_ASSERT(entry);
    STK_ASSERT(stack);
    STK_ASSERT(stack_size);
    return reinterpret_cast<stk_task_t *>(AllocateTask(entry, arg, stack, stack_size, ACCESS_USER));
}

void stk_task_set_weight(stk_task_t *task, int32_t weight)
{
    STK_ASSERT(task);
    STK_ASSERT(weight > 0);
    reinterpret_cast<TaskWrapper *>(task)->SetWeight(weight);
}

void stk_task_destroy(stk_task_t *task)
{
    STK_ASSERT(task);
    FreeTask(reinterpret_cast<TaskWrapper *>(task));
}

// ---------------------------------------------------------------------------
// Kernel services (available inside tasks)
// ---------------------------------------------------------------------------
size_t  stk_tid(void)             { return stk::GetTid(); }
int64_t stk_ticks(void)           { return stk::GetTicks(); }
int32_t stk_tick_resolution(void) { return stk::GetTickResolution(); }
int64_t stk_time_now_ms(void)     { return stk::GetTimeNowMsec(); }
void    stk_delay_ms(uint32_t ms) { stk::Delay(ms); }
void    stk_sleep_ms(uint32_t ms) { stk::Sleep(ms); }
void    stk_yield(void)           { stk::Yield(); }

// ---------------------------------------------------------------------------
// Thread-Local Storage (TLS) API
// ---------------------------------------------------------------------------
void *stk_tls_get(void)
{
    return stk::GetTlsPtr<void *>();
}

void stk_tls_set(void *ptr)
{
    stk::SetTlsPtr(ptr);
}

// ---------------------------------------------------------------------------
// Critical Section - Manual Enter/Exit
// ---------------------------------------------------------------------------
void stk_enter_critical_section(void)
{
    stk::EnterCriticalSection();
}

void stk_exit_critical_section(void)
{
    stk::ExitCriticalSection();
}

} // extern "C"
