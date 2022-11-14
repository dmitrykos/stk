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

#include "arch/stk_arch_common.h"
#include "arch/arm/cortex-m/stk_arch_arm-cortex-m.h"

using namespace stk;

#define STK_CORTEX_M_CRITICAL_SESSION_START(SES) SES = __get_PRIMASK(); __disable_irq()
#define STK_CORTEX_M_CRITICAL_SESSION_END(SES) __set_PRIMASK(SES)
#define STK_CORTEX_M_DISABLE_INTERRUPTS() __asm volatile("CPSID i")
#define STK_CORTEX_M_ENABLE_INTERRUPTS() __asm volatile("CPSIE i")
#define STK_CORTEX_M_PRIVILEGED_MODE_ON() __set_CONTROL(__get_CONTROL() & ~CONTROL_nPRIV_Msk)
#define STK_CORTEX_M_PRIVILEGED_MODE_OFF() __set_CONTROL(__get_CONTROL() | CONTROL_nPRIV_Msk)
#define STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE 0xFFFFFFFDUL
#define STK_CORTEX_M_ISR_PRIORITY_LOWEST 0xFF

//! If defined, assume CPU may support FPU.
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    #define STK_CORTEX_M_EXPECT_FPU
#endif

//! If defined, manage Link Register (LR) per task.
#if defined(__CORTEX_M) && (__CORTEX_M >= 3)
    #define STK_CORTEX_M_MANAGE_LR
#endif

//! Number of registers kept in stack.
#ifdef STK_CORTEX_M_MANAGE_LR
    #define STK_CORTEX_M_REGISTER_COUNT 17
#else
    #define STK_CORTEX_M_REGISTER_COUNT 16
#endif

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *first_stack, int32_t tick_resolution)
    {
        PlatformContext::Initialize(handler, first_stack, tick_resolution);

        m_started = false;
    }

    bool m_started; //!< started state
}
g_Context;

extern "C" void _STK_SYSTICK_HANDLER()
{
#if (defined(STM32F0) || defined(STM32F3) || defined(STM32F4)) && defined(HAL_MODULE_ENABLED)
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
}

extern "C" __stk_attr_naked void _STK_PENDSV_HANDLER()
{
    STK_CORTEX_M_DISABLE_INTERRUPTS();

    // get Process Stack Pointer (PSP) of the current stack
    __asm volatile("MRS r0, psp");

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
    __asm volatile("STMDB   r0!, {r4-r11, LR}");
#else
    // note: STMIA is limited to r0-r7 range, therefore save via stack memory
    __asm volatile(
    "SUB    r0, #16         \n"
    "STMIA  r0!, {r4-r7}    \n"
    "MOV    r4, r8          \n"
    "MOV    r5, r9          \n"
    "MOV    r6, r10         \n"
    "MOV    r7, r11         \n"
    "SUB    r0, #32         \n"
    "STMIA  r0!, {r4-r7}    \n"
    "SUB    r0, #16         \n");
#endif

    // save stack memory to the idle stack
    __asm volatile(
    "LDR    r1, %[stack]    \n"
    "STR    r0, [r1]        \n"
    ::
    [stack] "o" (g_Context.m_stack_idle));

    // set active stack
    __asm volatile(
    "LDR    r1, %[stack]    \n"
    "LDR    r0, [r1]        \n" // load
    ::
    [stack] "o" (g_Context.m_stack_active));

    // load general registers from the active stack
#ifdef STK_CORTEX_M_MANAGE_LR
    __asm volatile("LDMIA   r0!, {r4-r11, LR}");
#else
    // note: LDMIA is limited to r0-r7 range, therefore load via stack memory
    __asm volatile(
    "LDMIA  r0!,{r4-r7} \n"
    "MOV    r8, r4      \n"
    "MOV    r9, r5      \n"
    "MOV    r10, r6     \n"
    "MOV    r11, r7     \n"
    "LDMIA  r0!,{r4-r7} \n");
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
    __asm volatile("MSR psp, r0");

    STK_CORTEX_M_ENABLE_INTERRUPTS();

    __asm volatile("BX  LR");
}

static __stk_attr_naked void OnTaskRun()
{
    STK_CORTEX_M_DISABLE_INTERRUPTS();

    // set active stack
    __asm volatile(
    "LDR    r1, %[stack]    \n"
    "LDR    r0, [r1]        \n"
    ::
    [stack] "o" (g_Context.m_stack_active));

    // load general registers from the active stack
#ifdef STK_CORTEX_M_MANAGE_LR
    __asm volatile("LDMIA   r0!, {r4-r11, LR}");
#else
    // note: LDMIA is limited to r0-r7 range, therefore load via stack memory
    __asm volatile(
    "LDMIA  r0!, {r4-r7}    \n"
    "MOV    r8, r4          \n"
    "MOV    r9, r5          \n"
    "MOV    r10, r6         \n"
    "MOV    r11, r7         \n"
    "LDMIA  r0!, {r4-r7}    \n");

    // return to the Thread mode, use PSP state & stack
    __asm volatile("LDR LR, =%0" :: "i" (STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE));
#endif

    // set Process Stack Pointer (PSP) of the new active stack
    __asm volatile("MSR psp, r0");

    STK_CORTEX_M_ENABLE_INTERRUPTS();

    __asm volatile("BX  LR");
}

static __stk_forceinline void StartFirstTask()
{
    __asm volatile("SVC #0");
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

extern "C" void SVC_Handler_Main(size_t *svc_args)
{
    // Stack contains: r0, r1, r2, r3, r12, r14, the return address and xPSR, First argument (r0) is svc_args[0]
    char svc_number = ((char *)svc_args[6])[-2];

    switch (svc_number)
    {
    case 0: {
        // disallow any duplicate attempt
    	STK_ASSERT(!g_Context.m_started);
        if (g_Context.m_started)
            return;

        g_Context.m_started = true;

        // FPU
        EnableFullFpuAccess();

        // clear FPU usage status if FPU was used before kernel start
        ClearFpuState();

        // notify kernel
        g_Context.m_handler->OnStart();

        // execute first task
        OnTaskRun();
        break; }
    default:
    	STK_ASSERT(false);
        break;
    }
}

// source:
// ARM: How to Write an SVC Function, https://developer.arm.com/documentation/ka004005/latest
extern "C" __stk_attr_naked void SVC_Handler()
{
    __asm volatile(
    ".global SVC_Handler_Main   \n"
    "TST    LR, #4              \n"
    "ITE    EQ                  \n"
    "MRSEQ  r0, MSP             \n"
    "MRSNE  r0, PSP             \n"
    "B      SVC_Handler_Main    \n");
}

static void OnTaskExit()
{
    volatile static int i = 0;
    while (true)
    {
        ++i;
        __asm volatile("NOP");
    }
}

void PlatformArmCortexM::Start(IEventHandler *event_handler, uint32_t tick_resolution, IKernelTask *first_task)
{
    g_Context.Initialize(event_handler, first_task->GetUserStack(), tick_resolution);
    
    NVIC_SetPriority(PendSV_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);
    NVIC_SetPriority(SysTick_IRQn, STK_CORTEX_M_ISR_PRIORITY_LOWEST);

    uint32_t result = SysTick_Config((uint32_t)((int64_t)SystemCoreClock * tick_resolution / 1000000));
    STK_ASSERT(result == 0);
    (void)result;

    StartFirstTask();
}

bool PlatformArmCortexM::InitStack(Stack *stack, ITask *user_task)
{
    uint32_t stack_size = user_task->GetStackSize();
    if (stack_size <= STK_CORTEX_M_REGISTER_COUNT)
        return false;

    size_t *stack_top = user_task->GetStack() + stack_size;

    // initialize stack memory
    for (int32_t i = -stack_size; i <= -4; ++i)
    {
        stack_top[i] = (size_t)(sizeof(size_t) <= 4 ? 0xdeadbeef : 0xdeadbeefdeadbeef);
    }

    size_t xPSR = (1 << 24); // set T bit of EPSR sub-regiser to enable execution of instructions (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/special-purpose-program-status-registers--xpsr-)
    size_t PC   = (size_t)user_task->GetFunc() & ~0x1UL; // "Bit [0] is always 0, so instructions are always aligned to halfword boundaries" (https://developer.arm.com/documentation/ddi0413/c/programmer-s-model/registers/general-purpose-registers)
    size_t LR   = (size_t)OnTaskExit;
    size_t R0   = (size_t)user_task->GetFuncUserData();

    // xPSR, PC, LR, R12, R3, R2, R1, R0
    // -1    -2  -3  -4   -5  -6  -7  -8
    stack_top[-1] = xPSR;
    stack_top[-2] = PC;
    stack_top[-3] = LR;
    stack_top[-8] = R0;

    // Exception exit value (LR) if FP is present
#ifdef STK_CORTEX_M_MANAGE_LR
    stack_top[-9] = STK_CORTEX_M_EXCEPTION_EXIT_THREAD_PSP_MODE;
#endif

    // initialize Stack Pointer (SP)
    stack->SP = (size_t)(stack_top - STK_CORTEX_M_REGISTER_COUNT);

    return true;
}

void PlatformArmCortexM::SwitchContext()
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
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
