/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

// note: If missing, this header must be customized (get it in the root of the source folder) and
//       copied to the /include folder manually.
#include "stk_config.h"

#ifdef _STK_ARCH_ARM_CORTEX_M

#include <setjmp.h>

#include "arch/stk_arch_common.h"
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"

using namespace stk;

#define STK_CORTEX_M_CRITICAL_SESSION_START(SES) SES = __get_PRIMASK(); __disable_irq()
#define STK_CORTEX_M_CRITICAL_SESSION_END(SES) __set_PRIMASK(SES)
#define STK_CORTEX_M_DISABLE_INTERRUPTS() __disable_irq()
#define STK_CORTEX_M_ENABLE_INTERRUPTS() __enable_irq()
#ifdef CONTROL_nPRIV_Msk
    #define STK_CORTEX_M_PRIVILEGED_MODE_ON() __set_CONTROL(__get_CONTROL() & ~CONTROL_nPRIV_Msk)
    #define STK_CORTEX_M_PRIVILEGED_MODE_OFF() __set_CONTROL(__get_CONTROL() | CONTROL_nPRIV_Msk)
#else
    #define STK_CORTEX_M_PRIVILEGED_MODE_ON() ((void)0)
    #define STK_CORTEX_M_PRIVILEGED_MODE_OFF() ((void)0)
#endif
#define STK_CORTEX_M_EXCEPTION_EXIT_THREAD_MSP_MODE 0xFFFFFFF9
#define STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE 0xFFFFFFFD
#define STK_CORTEX_M_ISR_PRIORITY_LOWEST 0xFF
#define STK_CORTEX_M_EXIT_FROM_HANDLER() __asm volatile("BX LR")
#define STK_CORTEX_M_START_SCHEDULING() __asm volatile("SVC #0")

//! Do sanity check for a compiler define, __CORTEX_M must be defined.
#ifndef __CORTEX_M
#error Expecting __CORTEX_M with value corresponding to Cortex-M model (0, 3, 4, ...)!
#endif

//! If defined, assume CPU may support FPU.
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    #define STK_CORTEX_M_EXPECT_FPU
#endif

//! If defined, manage Link Register (LR) per task.
#if (__CORTEX_M >= 3)
    #define STK_CORTEX_M_MANAGE_LR
#endif

//! Number of registers kept in stack.
#ifdef STK_CORTEX_M_MANAGE_LR
    #define STK_CORTEX_M_REGISTER_COUNT 17
#else
    #define STK_CORTEX_M_REGISTER_COUNT 16
#endif

//! SysTick_Handler
#ifndef _STK_SYSTICK_HANDLER
    #define _STK_SYSTICK_HANDLER SysTick_Handler
#endif

//! PendSV_Handler
#ifndef _STK_PENDSV_HANDLER
    #define _STK_PENDSV_HANDLER PendSV_Handler
#endif

//! SVC_Handler
#ifndef _STK_SVC_HANDLER
    #define _STK_SVC_HANDLER SVC_Handler
#endif

// Declarations:
extern "C" void SVC_Handler_Main(size_t *svc_args) __stk_attr_used; // __stk_attr_used required for Link-Time Optimization (-flto)

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *main_process, Stack *first_stack, int32_t tick_resolution)
    {
        PlatformContext::Initialize(handler, main_process, first_stack, tick_resolution);

        m_started = false;
        m_exiting    = false;
    }

    bool    m_started;  //!< 'true' when in started state
    bool    m_exiting;  //!< 'true' when is exiting the scheduling process
    jmp_buf m_exit_buf; //!< saved context of the exit point
}
g_Context;

extern "C" void _STK_SYSTICK_HANDLER()
{
#ifdef HAL_MODULE_ENABLED // STM32 HAL
    HAL_IncTick();
    // unfortunately STM32 HAL is starting SysTick on its initialization that
    // will cause a crash on NULL, therefore use additional check
    if (g_Context.m_started)
#else
    // make sure SysTick is enabled by the Kernel::Start(), disable its start anywhere else
    STK_ASSERT(g_Context.m_started);
    STK_ASSERT(g_Context.m_handler != NULL);
#endif
    g_Context.m_handler->OnSysTick(&g_Context.m_stack_idle, &g_Context.m_stack_active);
    __DSB();
}

__stk_forceinline void SaveStackIdle()
{
    // get Process Stack Pointer (PSP) of the current stack
    __asm volatile(
    "MRS        r0, psp         \n");

    // save FP general registers
#ifdef STK_CORTEX_M_EXPECT_FPU
    __asm volatile(
    "TST        LR, #16         \n" // test LR for 0xffffffe_, e.g. Thread mode with FP data
    "IT         EQ              \n"
    "VSTMDBEQ   r0!, {s16-s31}  \n" // store 16 SP registers
    );
#endif

    // save general registers to the stack memory
    // note: for Cortex-M3 and higher save LR to keep correct Thread state of the task when it is restored
#ifdef STK_CORTEX_M_MANAGE_LR
    __asm volatile(
    "STMDB      r0!, {r4-r11, LR}\n");
#else
    // note: STMIA is limited to r0-r7 range, therefore save via stack memory
    __asm volatile(
    "SUB        r0, #16         \n"
    "STMIA      r0!, {r4-r7}    \n"
    "MOV        r4, r8          \n"
    "MOV        r5, r9          \n"
    "MOV        r6, r10         \n"
    "MOV        r7, r11         \n"
    "SUB        r0, #32         \n"
    "STMIA      r0!, {r4-r7}    \n"
    "SUB        r0, #16         \n");
#endif

    // save stack memory to the idle stack
    __asm volatile(
    "LDR        r1, %[stack]    \n"
    "STR        r0, [r1]        \n"
    ::
    [stack] "o" (g_Context.m_stack_idle));
}

__stk_forceinline void LoadStackActive()
{
    // set active stack
    __asm volatile(
    "LDR        r1, %[stack]    \n"
    "LDR        r0, [r1]        \n" // load
    ::
    [stack] "o" (g_Context.m_stack_active));

    // load general registers from the active stack
#ifdef STK_CORTEX_M_MANAGE_LR
    __asm volatile(
    "LDMIA      r0!, {r4-r11, LR}\n");
#else
    // note: LDMIA is limited to r0-r7 range, therefore load via stack memory
    __asm volatile(
    "LDMIA      r0!, {r4-r7}    \n"
    "MOV        r8, r4          \n"
    "MOV        r9, r5          \n"
    "MOV        r10, r6         \n"
    "MOV        r11, r7         \n"
    "LDMIA      r0!, {r4-r7}    \n");
#endif

    // load FP general registers
#ifdef STK_CORTEX_M_EXPECT_FPU
    __asm volatile(
    "TST        LR, #16         \n" // test LR for 0xffffffe_, e.g. Thread mode with FP data
    "IT         EQ              \n" // if result is positive
    "VLDMIAEQ   r0!, {s16-s31}  \n" // restore FP registers
    );
#endif

    // set Process Stack Pointer (PSP) of the new active stack
    __asm volatile(
    "MSR        psp, r0         \n");
}

__stk_forceinline void OnTaskSwitch()
{
    STK_CORTEX_M_DISABLE_INTERRUPTS();

    SaveStackIdle();
    LoadStackActive();

    STK_CORTEX_M_ENABLE_INTERRUPTS();

    STK_CORTEX_M_EXIT_FROM_HANDLER();
}

extern "C" __stk_attr_naked void _STK_PENDSV_HANDLER()
{
    OnTaskSwitch();
}

__stk_forceinline void OnTaskRun()
{
    STK_CORTEX_M_DISABLE_INTERRUPTS();

    LoadStackActive();

#ifndef STK_CORTEX_M_MANAGE_LR
    // M0: set LR to the Thread mode, use PSP state & stack
    __asm volatile(
    "LDR    r0, =%0         \n"
    "MOV    LR, r0          \n"
    ::
    "i" (STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE));
#endif

    STK_CORTEX_M_ENABLE_INTERRUPTS();

    STK_CORTEX_M_EXIT_FROM_HANDLER();
}

static __stk_forceinline void ClearFpuState()
{
#ifdef STK_CORTEX_M_EXPECT_FPU
    __set_CONTROL(__get_CONTROL() & ~CONTROL_FPCA_Msk);
#endif
}

static __stk_forceinline void EnableFullFpuAccess()
{
#ifdef STK_CORTEX_M_EXPECT_FPU
    // enable FPU CP10/CP11 Secure and Non-secure register access
    SCB->CPACR |= (0b11 << 20) | (0b11 << 22);
#endif
}

static void StartScheduling()
{
    // disallow any duplicate attempt
    STK_ASSERT(!g_Context.m_started);
    if (g_Context.m_started)
        return;

    // FPU
    EnableFullFpuAccess();

    // clear FPU usage status if FPU was used before kernel start
    ClearFpuState();

    // set priority
    NVIC_SetPriority(PendSV_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);
    NVIC_SetPriority(SysTick_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);

    // notify kernel
    g_Context.m_handler->OnStart();

    // schedule ticks
    uint32_t result = SysTick_Config((uint32_t)((int64_t)SystemCoreClock * g_Context.m_tick_resolution / 1000000));
    STK_ASSERT(result == 0);
    (void)result;

    g_Context.m_started = true;
}

void SVC_Handler_Main(size_t *stack)
{
    // Stack contains: r0, r1, r2, r3, r12, r14, the return address and xPSR, First argument (r0) is svc_args[0]
    // - R0 = stack[0]
    // - R1 = stack[1]
    // - R2 = stack[2]
    // - R3 = stack[3]
    // - R12 = stack[4]
    // - LR = stack[5]
    // - PC = stack[6]
    // - xPSR= stack[7]
    char svc_arg = ((char *)stack[6])[-2];

    switch (svc_arg)
    {
    case 0: {
        StartScheduling();
        OnTaskRun();
        break; }
    default:
        STK_ASSERT(false);
        break;
    }
}

// source:
// ARM: How to Write an SVC Function, https://developer.arm.com/documentation/ka004005/latest
extern "C" __stk_attr_naked void _STK_SVC_HANDLER()
{
#if (__CORTEX_M >= 3)
    __asm volatile(
    ".global SVC_Handler_Main   \n"
    "TST    LR, #4              \n" // if (LR & 4)
    "ITE    EQ                  \n"
    "MRSEQ  r0, MSP             \n" // r0 = MSP
    "MRSNE  r0, PSP             \n" // else r0 = PSP
    "B      SVC_Handler_Main    \n");
#else
    __asm volatile(
    ".syntax unified            \n"
    ".global SVC_Handler_Main   \n"
    "MOV    r0, LR              \n" // r0 = LR
    "LSLS   r0, r0, #29         \n" // if (r0 & 4)
    "BMI    .ELSE               \n"
    "MRS    r0, MSP             \n" // r0 = MSP
    "B      SVC_Handler_Main    \n"
    ".ELSE:                     \n"
    "MRS    r0, PSP             \n" // else r0 = PSP
    "B      SVC_Handler_Main    \n");
#endif
}

static void OnTaskExit()
{
    uint32_t mutex;
    STK_CORTEX_M_CRITICAL_SESSION_START(mutex);

    g_Context.m_handler->OnTaskExit(g_Context.m_stack_active);

    STK_CORTEX_M_CRITICAL_SESSION_END(mutex);

    while (true)
    {
        __asm volatile("NOP");
    }
}

static void OnSchedulerExit()
{
    __set_CONTROL(0); // switch to MSP
    __set_PSP(0);     // clear PSP (for a clean register state)

    // jump to the exit from the IKernel::Start()
    longjmp(g_Context.m_exit_buf, 0);
}

void PlatformArmCortexM::Start(IEventHandler *event_handler, uint32_t tick_resolution, IKernelTask *first_task, Stack *main_process)
{
    g_Context.Initialize(event_handler, main_process, first_task->GetUserStack(), tick_resolution);
    g_Context.m_exiting = false;

    // save jump location of the Exit trap
    setjmp(g_Context.m_exit_buf);
    if (g_Context.m_exiting)
        return;

    STK_CORTEX_M_START_SCHEDULING();
}

bool PlatformArmCortexM::InitStack(Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    uint32_t stack_size = stack_memory->GetStackSize();
    if (stack_size <= STK_CORTEX_M_REGISTER_COUNT)
        return false;

    size_t *stack_top = stack_memory->GetStack() + stack_size;

    // initialize Stack Pointer (SP)
    stack->SP = (size_t)(stack_top - STK_CORTEX_M_REGISTER_COUNT);

    // initialize stack memory
    for (int32_t i = -stack_size; i <= -4; ++i)
    {
        stack_top[i] = (size_t)(sizeof(size_t) <= 4 ? 0xdeadbeef : 0xdeadbeefdeadbeef);
    }

    // xPSR, PC, LR, R12, R3, R2, R1, R0
    // -1    -2  -3  -4   -5  -6  -7  -8

    size_t xPSR = (1 << 24); // set T bit of EPSR sub-regiser to enable execution of instructions (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/special-purpose-program-status-registers--xpsr-)
    size_t PC, LR, R0;

    stack_top[-1] = xPSR;

    // initialize registers for the user task's first start
    if (user_task != NULL)
    {
        PC = (size_t)user_task->GetFunc() & ~0x1UL; // "Bit [0] is always 0, so instructions are always aligned to halfword boundaries" (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/general-purpose-registers)
        LR = (size_t)OnTaskExit;
        R0 = (size_t)user_task->GetFuncUserData();
    }
    else
    {
        PC = (size_t)OnSchedulerExit & ~0x1UL; // scheduler's Exit trap
        LR = (size_t)OnSchedulerExit;
        R0 = 0;
    }

    stack_top[-2] = PC;
    stack_top[-3] = LR;
    stack_top[-8] = R0;

    // Exception exit value (LR) if FP is present
#ifdef STK_CORTEX_M_MANAGE_LR
    stack_top[-9] = STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE;
#endif

    return true;
}

void PlatformArmCortexM::SwitchContext()
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __ISB();
}

void PlatformArmCortexM::StopScheduling()
{
    // stop and clear SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    g_Context.m_started = false;
    g_Context.m_exiting = true;

    // load context of the Exit trap
    OnTaskRun();
}

int32_t PlatformArmCortexM::GetTickResolution() const
{
    return g_Context.m_tick_resolution;
}

void PlatformArmCortexM::SetAccessMode(EAccessMode mode)
{
    if (mode == ACCESS_PRIVILEGED)
        STK_CORTEX_M_PRIVILEGED_MODE_ON();
    else
        STK_CORTEX_M_PRIVILEGED_MODE_OFF();
}

#endif // _STK_ARCH_ARM_CORTEX_M
