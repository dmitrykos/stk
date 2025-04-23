/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

// note: If missing, this header must be customized (get it in the root of the source folder) and
//       copied to the /include folder manually.
#include "stk_config.h"

#ifdef _STK_ARCH_ARM_CORTEX_M

#include <stdlib.h>
#include <setjmp.h>

#include "arch/stk_arch_common.h"
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"

using namespace stk;

#define STK_CORTEX_M_CRITICAL_SECTION_START(SES) do { SES = __get_PRIMASK(); __disable_irq(); } while (0)
#define STK_CORTEX_M_CRITICAL_SECTION_END(SES) __set_PRIMASK(SES)
#define STK_CORTEX_M_DISABLE_INTERRUPTS() __disable_irq()
#define STK_CORTEX_M_ENABLE_INTERRUPTS() __enable_irq()
#ifdef CONTROL_nPRIV_Msk
    #define STK_CORTEX_M_PRIVILEGED_MODE_ON() __set_CONTROL(__get_CONTROL() & ~CONTROL_nPRIV_Msk)
    #define STK_CORTEX_M_PRIVILEGED_MODE_OFF() __set_CONTROL(__get_CONTROL() | CONTROL_nPRIV_Msk)
#else
    #define STK_CORTEX_M_PRIVILEGED_MODE_ON() ((void)0)
    #define STK_CORTEX_M_PRIVILEGED_MODE_OFF() ((void)0)
#endif
#define STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE 0xFFFFFFFD
#define STK_CORTEX_M_ISR_PRIORITY_LOWEST 0xFF
#define STK_CORTEX_M_EXIT_FROM_HANDLER() __asm volatile("BX LR")
#define STK_CORTEX_M_START_SCHEDULING() __asm volatile("SVC #0")
#define STK_CORTEX_M_FORCE_SWITCH() __asm volatile("SVC #1")

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

/*! \brief Get SP of the calling process.
*/
__stk_forceinline size_t GetCallerSP()
{
    return __get_PSP();
}

/*! \brief Switch context by scheduling PendSV interrupt.
*/
__stk_forceinline void ScheduleContextSwitch()
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // schedule PendSV interrupt
    __ISB();                             // flush instructions cache
}

//! Platform events overrider.
static IPlatform::IEventOverrider *g_Overrider = NULL;

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, int32_t resolution_us)
    {
        PlatformContext::Initialize(handler, exit_trap, resolution_us);

        m_started     = false;
        m_exiting     = false;
        m_csu         = 0;
        m_csu_nesting = 0;
    }

    __stk_forceinline void OnTick()
    {
        STK_CORTEX_M_DISABLE_INTERRUPTS();

        if (m_handler->OnTick(&m_stack_idle, &m_stack_active))
        {
            ScheduleContextSwitch();
        }

        STK_CORTEX_M_ENABLE_INTERRUPTS();
    }

    __stk_forceinline void EnterCriticalSection()
    {
        if (m_csu_nesting == 0)
            STK_CORTEX_M_CRITICAL_SECTION_START(m_csu);

        ++m_csu_nesting;
    }

    __stk_forceinline void ExitCriticalSection()
    {
        STK_ASSERT(m_csu_nesting != 0);

        --m_csu_nesting;

        if (m_csu_nesting == 0)
            STK_CORTEX_M_CRITICAL_SECTION_END(m_csu);
    }

    bool     m_started;     //!< 'true' when in started state
    bool     m_exiting;     //!< 'true' when is exiting the scheduling process
    uint32_t m_csu;         //!< user critical session
    uint32_t m_csu_nesting; //!< depth of user critical session nesting
    jmp_buf  m_exit_buf;    //!< saved context of the exit point
}
g_Context;

void PlatformArmCortexM::ProcessTick()
{
    g_Context.OnTick();
}

extern "C" void _STK_SYSTICK_HANDLER()
{
#ifdef HAL_MODULE_ENABLED // STM32 HAL
    // make sure STM32 HAL get timing information as it depends on SysTick in delaying procedures
    HAL_IncTick();

    // STM32 HAL is starting SysTick on its initialization that will cause a crash on NULL,
    // therefore use additional check if HAL_MODULE_ENABLED is defined
    if (g_Context.m_started)
    {
#else
    {
        // make sure SysTick is enabled by the Kernel::Start(), disable its start anywhere else
        STK_ASSERT(g_Context.m_started);
        STK_ASSERT(g_Context.m_handler != NULL);
#endif
        g_Context.OnTick();
    }
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

    // save PSP of the idle stack
    __asm volatile(
    "LDR        r1, %0          \n"
    "STR        r0, [r1]        \n"
    : /* output: none */
    : "m" (g_Context.m_stack_idle)
    : /* clobbers: none */);
}

__stk_forceinline void LoadStackActive()
{
    // load PSP of the active stack
    __asm volatile(
    "LDR        r1, %0          \n"
    "LDR        r0, [r1]        \n" // load
    : /* output: none */
    : "m" (g_Context.m_stack_active)
    : /* clobbers: none */);

    // load general registers from the stack memory
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

extern "C" __stk_attr_naked void _STK_PENDSV_HANDLER()
{
    STK_CORTEX_M_DISABLE_INTERRUPTS();

    SaveStackIdle();
    LoadStackActive();

    STK_CORTEX_M_ENABLE_INTERRUPTS();

    STK_CORTEX_M_EXIT_FROM_HANDLER();
}

__stk_forceinline void OnTaskRun()
{
    // note: STK_CORTEX_M_DISABLE_INTERRUPTS() must be called prior calling this function

    LoadStackActive();

#ifndef STK_CORTEX_M_MANAGE_LR
    // M0: set LR to the Thread mode, use PSP state & stack
    __asm volatile(
    "LDR    r0, =%0         \n"
    "MOV    LR, r0          \n"
    : /* output: none */
    : "i" (STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE)
    : /* clobbers: none */);
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
    // FPU
    EnableFullFpuAccess();

    // clear FPU usage status if FPU was used before kernel start
    ClearFpuState();

    // notify kernel
    g_Context.m_handler->OnStart(&g_Context.m_stack_active);

    // schedule ticks
    uint32_t result = SysTick_Config((uint32_t)STK_TIME_TO_CPU_TICKS_USEC(SystemCoreClock, g_Context.m_tick_resolution));
    STK_ASSERT(result == 0);
    (void)result;

    // set priority (after SysTick_Config because it may change SysTick priority)
    NVIC_SetPriority(PendSV_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);
    NVIC_SetPriority(SysTick_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);

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
        // disallow any duplicate attempt
        STK_ASSERT(!g_Context.m_started);
        if (g_Context.m_started)
            return;

        STK_CORTEX_M_DISABLE_INTERRUPTS();
        StartScheduling();
        OnTaskRun();
        break; }

    default: {
        STK_ASSERT(false);
        break; }
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
    "BL     SVC_Handler_Main    \n");
#else
    __asm volatile(
    ".syntax unified            \n"
    ".global SVC_Handler_Main   \n"
    "MOV    r0, LR              \n" // r0 = LR
    "LSLS   r0, r0, #29         \n" // if (r0 & 4)
    "BMI    .ELSE               \n"
    "MRS    r0, MSP             \n" // r0 = MSP
    "BL     SVC_Handler_Main    \n"
    ".ELSE:                     \n"
    "MRS    r0, PSP             \n" // else r0 = PSP
    "BL     SVC_Handler_Main    \n");
#endif
}

static void OnTaskExit()
{
    uint32_t cs;
    STK_CORTEX_M_CRITICAL_SECTION_START(cs);

    g_Context.m_handler->OnTaskExit(g_Context.m_stack_active);

    STK_CORTEX_M_CRITICAL_SECTION_END(cs);

    for (;;)
    {
        __WFI(); // enter standby mode until time slot expires
    }
}

static void OnSchedulerSleep()
{
#if STK_SEGGER_SYSVIEW
    SEGGER_SYSVIEW_OnIdle();
#endif

    for (;;)
    {
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk; // disable deep-sleep, go into a WAIT mode (sleep)
        __DSB();                            // ensure store takes effect (see ARM info)

        __WFI();                            // enter sleep mode
        __ISB();
    }
}

static void OnSchedulerSleepOverride()
{
    if (!g_Overrider->OnSleep())
        OnSchedulerSleep();
}

static void OnSchedulerExit()
{
    __set_CONTROL(0); // switch to MSP
    __set_PSP(0);     // clear PSP (for a clean register state)

    // jump to the exit from the IKernel::Start()
    longjmp(g_Context.m_exit_buf, 0);
}

void PlatformArmCortexM::Start(IEventHandler *event_handler, uint32_t resolution_us, Stack *exit_trap)
{
    g_Context.Initialize(event_handler, exit_trap, resolution_us);
    g_Context.m_exiting = false;

    // save jump location of the Exit trap
    setjmp(g_Context.m_exit_buf);
    if (g_Context.m_exiting)
        return;

    STK_CORTEX_M_START_SCHEDULING();
}

bool PlatformArmCortexM::InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    STK_ASSERT(stack_memory->GetStackSize() > STK_CORTEX_M_REGISTER_COUNT);

    // initialize stack memory
    size_t *stack_top = g_Context.InitStackMemory(stack_memory);

    // initialize Stack Pointer (SP)
    stack->SP = (size_t)(stack_top - STK_CORTEX_M_REGISTER_COUNT);

    // xPSR, PC, LR, R12, R3, R2, R1, R0
    // -1    -2  -3  -4   -5  -6  -7  -8

    size_t xPSR = (1 << 24); // set T bit of EPSR sub-regiser to enable execution of instructions (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/special-purpose-program-status-registers--xpsr-)
    size_t PC, LR, R0;

    stack_top[-1] = xPSR;

    // initialize registers for the user task's first start
    switch (stack_type)
    {
    case STACK_USER_TASK: {
        PC = (size_t)user_task->GetFunc() & ~0x1UL; // "Bit [0] is always 0, so instructions are always aligned to halfword boundaries" (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/general-purpose-registers)
        LR = (size_t)OnTaskExit;
        R0 = (size_t)user_task->GetFuncUserData();
        break; }

    case STACK_SLEEP_TRAP: {
        PC = (size_t)(g_Overrider != NULL ? OnSchedulerSleepOverride : OnSchedulerSleep) & ~0x1UL;
        LR = (size_t)STK_STACK_MEMORY_FILLER; // should not attempt to exit
        R0 = 0;
        break; }

    case STACK_EXIT_TRAP: {
        PC = (size_t)OnSchedulerExit & ~0x1UL;
        LR = (size_t)STK_STACK_MEMORY_FILLER; // should not attempt to exit
        R0 = 0;
        break; }

    default:
        return false;
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

static void SysTick_Stop()
{
    SysTick->CTRL = 0;
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
}

void PlatformArmCortexM::Stop()
{
    // stop SysTick timer
    SysTick_Stop();

    // clear pending PendSV exception
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;

    g_Context.m_started = false;
    g_Context.m_exiting = true;

    // make sure all assignments are set and executed
    __DSB();
    __ISB();

    // load context of the Exit trap
    STK_CORTEX_M_DISABLE_INTERRUPTS();
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

void PlatformArmCortexM::SwitchToNext()
{
    g_Context.m_handler->OnTaskSwitch(::GetCallerSP());
}

void PlatformArmCortexM::SleepTicks(uint32_t ticks)
{
    g_Context.m_handler->OnTaskSleep(::GetCallerSP(), ticks);
}

void PlatformArmCortexM::ProcessHardFault()
{
    if ((g_Overrider == NULL) || !g_Overrider->OnHardFault())
    {
        exit(1);
    }
}

void PlatformArmCortexM::SetEventOverrider(IEventOverrider *overrider)
{
    STK_ASSERT(!g_Context.m_started);
    g_Overrider = overrider;
}

size_t PlatformArmCortexM::GetCallerSP()
{
    return ::GetCallerSP();
}

void stk::EnterCriticalSection()
{
    g_Context.EnterCriticalSection();
}

void stk::ExitCriticalSection()
{
    g_Context.ExitCriticalSection();
}

#endif // _STK_ARCH_ARM_CORTEX_M
