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

#ifdef _STK_ARCH_RISC_V

#include <stdlib.h>
#include <setjmp.h>

#include "arch/stk_arch_common.h"
#include "arch/risc-v/stk_arch_risc-v.h"

using namespace stk;

// RISC-V does not have PendSV interrupt functionality similar to Arm Cortex-M, so reserve
// this functionality for the future extension
//#define _STK_RISCV_USE_PENDSV
#ifdef _STK_RISCV_USE_PENDSV
#error RISC-V has no PendSV interrupt functionality similar to Arm Cortex-M!
#endif

// CLINT
// Details: https://github.com/riscv/riscv-aclint/blob/main/riscv-aclint.adoc
#ifndef _STK_RISCV_CLINT_BASE_ADDR
    #define _STK_RISCV_CLINT_BASE_ADDR (0x2000000)
#endif
#ifndef STK_RISCV_CLINT_MSIP_ADDR
    #define STK_RISCV_CLINT_MSIP_ADDR (_STK_RISCV_CLINT_BASE_ADDR + 0x0000) // 4-byte value, 1 per hart
#endif
#ifndef STK_RISCV_CLINT_MTIMECMP_ADDR
    #define STK_RISCV_CLINT_MTIMECMP_ADDR (_STK_RISCV_CLINT_BASE_ADDR + 0x4000) // 8-byte value, 1 per hart
#endif
#ifndef STK_RISCV_CLINT_MTIME_ADDR
    #define STK_RISCV_CLINT_MTIME_ADDR (_STK_RISCV_CLINT_BASE_ADDR + 0xBFF8) // 8-byte value, global
#endif

#define STK_RISCV_CRITICAL_SECTION_START(SES) do { SES = ::HW_EnterCriticalSection(); } while (0)
#define STK_RISCV_CRITICAL_SECTION_END(SES) ::HW_ExitCriticalSection(SES)

#define STK_RISCV_DISABLE_INTERRUPTS() ::HW_DisableIrq()
#define STK_RISCV_ENABLE_INTERRUPTS() ::HW_EnableIrq()

#define STK_RISCV_WFI() __asm volatile("wfi")

#define STK_RISCV_PRIVILEGED_MODE_ON() ((void)0)
#define STK_RISCV_PRIVILEGED_MODE_OFF() ((void)0)

#define STK_RISCV_EXIT_FROM_HANDLER() __asm volatile("mret")

#define STK_RISCV_START_SCHEDULING() __asm volatile("ecall") // cause exception with RISCV_EXCP_ENVIRONMENT_CALL_FROM_M_MODE

#define STK_RISCV_ISR extern "C" STK_RISCV_ISR_SECTION __attribute__ ((interrupt ("machine")))

//! Use main stack for ISR handling.
#define STK_RISCV_USE_MAIN_STACK_FOR_ISR

//! Timer handler.
#ifndef _STK_SYSTICK_HANDLER
    #define _STK_SYSTICK_HANDLER riscv_mtvec_mti // see vector_table.h/vector_table.c
#endif

//! Exception handler.
#ifndef _STK_SVC_HANDLER
    #define _STK_SVC_HANDLER riscv_mtvec_exception // see vector_table.h/vector_table.c
#endif

#if (__riscv_flen == 0)
    #define STK_RISCV_FP 0
#else
    #define STK_RISCV_FP __riscv_flen
#endif

#if (__riscv_32e == 1)
    #define STK_RISCV_REGISTER_COUNT (15 + (STK_RISCV_FP != 0 ? 31 : 0))
#else
    #define STK_RISCV_REGISTER_COUNT (31 + (STK_RISCV_FP != 0 ? 31 : 0))
#endif

#define STK_SERVICE_SLOTS 2 // (0) mepc, (1) mstatus

#define STR(x) #x
#define XSTR(s) STR(s)

#if (__riscv_xlen == 32)
    #define REGBYTES XSTR(4)
    #define LREG     XSTR(lw)
    #define SREG     XSTR(sw)
#elif (__riscv_xlen == 64)
    #define REGBYTES XSTR(8)
    #define LREG     XSTR(ld)
    #define SREG     XSTR(sd)
#else
    #error Unsupported RISC-V platform!
#endif

#if (__riscv_flen == 32)
    #define FREGBYTES XSTR(4)
    #define FLREG     XSTR(flw)
    #define FSREG     XSTR(fsw)
#elif (__riscv_flen == 64)
    #define FREGBYTES XSTR(8)
    #define FLREG     XSTR(fld)
    #define FSREG     XSTR(fsd)
#elif (__riscv_flen != 0)
#error Unsupported FP register count!
#endif

#if (__riscv_32e == 1)
    #define FOFFSET XSTR(68) // FP stack offset = (17 * 4)
    #if (__riscv_flen == 0)
        #define REGSIZE XSTR(((15 + 2) * 4)) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus
    #else
        #if (__riscv_flen == 32)
        #define REGSIZE XSTR((((15 + 2) * 4) + (31 * 4))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #elif (__riscv_flen == 64)
        #define REGSIZE XSTR((((15 + 2) * 4) + (31 * 8))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #endif
    #endif
#elif (__riscv_xlen == 32)
    #define FOFFSET XSTR(132) // FP stack offset = (33 * 4)
    #if (__riscv_flen == 0)
        #define REGSIZE XSTR(((31 + 2) * 4)) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus
    #else
        #if (__riscv_flen == 32)
        #define REGSIZE XSTR((((31 + 2) * 4) + (31 * 4))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #elif (__riscv_flen == 64)
        #define REGSIZE XSTR((((31 + 2) * 4) + (31 * 8))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #endif
    #endif
#elif (__riscv_xlen == 64)
    #define FOFFSET XSTR(264) // FP stack offset = (33 * 8)
    #if (__riscv_flen == 0)
        #define REGSIZE XSTR(((31 + 2) * 8)) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus
    #else
        #if (__riscv_flen == 32)
        #define REGSIZE XSTR((((31 + 2) * 8) + (31 * 4))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #elif (__riscv_flen == 64)
        #define REGSIZE XSTR((((31 + 2) * 8) + (31 * 8))) // STK_RISCV_REGISTER_COUNT + 2 for mepc, mstatus + 32 fp registers
        #endif
    #endif
#endif

#define STK_RISCV_REG_INDEX(REG) (-((STK_RISCV_REGISTER_COUNT + 1) - REG))
#define STK_RISCV_SRV_INDEX(REG) (STK_RISCV_REG_INDEX(REG) - STK_SERVICE_SLOTS)

#ifdef __NEWLIB__
/*! \brief Placement new operator for allocation of Context.
*/
__stk_forceinline void *operator new(std::size_t, void *ptr) noexcept
{
    return ptr;
}
__stk_forceinline void operator delete(void*, void*) noexcept
{
    // required counterpart for new(), does nothing
}
#endif

static __stk_forceinline void HW_DisableIrq()
{
    __asm volatile("csrrci zero, mstatus, %0"
    : /* output: none */
    : "i"(MSTATUS_MIE)
    : /* clobbers: none */);
}

static __stk_forceinline void HW_EnableIrq()
{
    __asm volatile("csrrsi zero, mstatus, %0"
    : /* output: none */
    : "i"(MSTATUS_MIE)
    : /* clobbers: none */);
}

/*! \brief  Enter critical section.
    \return Session value which has to be supplied to HW_ExitCriticalSection().
*/
static __stk_forceinline size_t HW_EnterCriticalSection()
{
    size_t ses;

    __asm volatile("csrrci %0, mstatus, %1"
    : "=r"(ses)
    : "i"(MSTATUS_MIE)
    :  /* clobbers: none */);

    return ses;
}

/*! \brief     Exit critical section.
    \param[in] ses: Session value obtained by HW_EnterCriticalSection().
*/
static __stk_forceinline void HW_ExitCriticalSection(size_t ses)
{
    __asm volatile("csrrs zero, mstatus, %0"
    : /* output: none */
    : "r"(ses)
    : /* clobbers: none */);
}

/*! \brief  Get mtime.
    \return Ticks.
*/
static __stk_forceinline uint64_t HW_GetMtime()
{
#if ( __riscv_xlen > 32)
    return *((volatile uint64_t *)STK_RISCV_CLINT_MTIME_ADDR);
#else
    volatile uint32_t *mtime_hi = ((volatile uint32_t *)STK_RISCV_CLINT_MTIME_ADDR) + 1;
    volatile uint32_t *mtime_lo = ((volatile uint32_t *)STK_RISCV_CLINT_MTIME_ADDR);

    uint32_t hi, lo;
    do
    {
        hi = (*mtime_hi);
        lo = (*mtime_lo);
    }
    while (hi != (*mtime_hi)); // make sure mtime_hi did not tick when read mtime_lo

    return ((uint64_t)hi << 32) | lo;
#endif
}

/*! \brief     Set mtimecmp register.
    \param[in] advance: Time delay (ticks) till the next interrupt.
*/
static __stk_forceinline void HW_SetMtimecmp(uint64_t advance)
{
    uint64_t next = HW_GetMtime() + advance;

    uint32_t hart;
#ifdef STK_RISCV_CLINT_MTIMECMP_PER_HART
    hart = read_csr(mhartid);
#else
    hart = 0;
#endif
#if (__riscv_xlen == 64)
    ((volatile uint64_t *)STK_RISCV_CLINT_MTIMECMP_ADDR)[hart] = next;
#else
    volatile uint32_t *mtime_lo = (volatile uint32_t *)((uint64_t *)STK_RISCV_CLINT_MTIMECMP_ADDR + hart);
    volatile uint32_t *mtime_hi = mtime_lo + 1;

    STK_ASSERT(((uintptr_t)mtime_lo & 3) == 0);
    STK_ASSERT(((uintptr_t)mtime_hi & 3) == 0);

    // prevent unexpected interrupt by setting some very large value to the high part
    // details: https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf, page 31
    (*mtime_hi) = ~0;

    (*mtime_lo) = (uint32_t)(next & 0xFFFFFFFF);
    (*mtime_hi) = (uint32_t)(next >> 32);
#endif
}

/*! \brief Get SP of the calling process.
*/
static __stk_forceinline size_t HW_GetCallerSP()
{
    size_t sp;

    // load SP into sp variable
    __asm volatile(
    SREG " sp, %0"
    : "=m"(sp)
    : /* input: none */
    : /* clobbers: none */);

    return sp;
}

/*! \brief Switch context by scheduling PendSV interrupt.
*/
static __stk_forceinline void ScheduleContextSwitch()
{
#ifdef _STK_RISCV_USE_PENDSV
    // TO-DO
#endif
}

//! ISR handler's stack memory.
#ifndef STK_RISCV_USE_MAIN_STACK_FOR_ISR
typedef StackMemoryDef<128> TIsrStackMemory;
static TIsrStackMemory::Type g_IsrStackMem = {};
#endif

//! Define _STK_SYSTEM_CLOCK_VAR privately by the driver if _STK_SYSTEM_CLOCK_EXTERNAL
//! is 0 or undefined.
#if !_STK_SYSTEM_CLOCK_EXTERNAL
volatile uint32_t _STK_SYSTEM_CLOCK_VAR = _STK_SYSTEM_CLOCK_FREQUENCY;
#endif

//! Internal context.
static struct Context : public PlatformContext
{
    void Initialize(IPlatform::IEventHandler *handler, Stack *exit_trap, int32_t resolution_us)
    {
        PlatformContext::Initialize(handler, exit_trap, resolution_us);

    #ifndef STK_RISCV_USE_MAIN_STACK_FOR_ISR
        m_stack_main.SP = (size_t)&g_IsrStackMem[TIsrStackMemory::SIZE];
    #else
        m_stack_main.SP = STK_STACK_MEMORY_FILLER;
    #endif
        m_stack_main.mode = ACCESS_PRIVILEGED;
        m_csu           = 0;
        m_csu_nesting   = 0;
        m_overrider     = NULL;
        m_specific      = NULL;
        m_tick_period   = STK_TIME_TO_CPU_TICKS_USEC(_STK_SYSTEM_CLOCK_VAR, resolution_us);
        m_starting      = false;
        m_started       = false;
        m_exiting       = false;
    }

    __stk_forceinline void OnTick()
    {
        if (m_handler->OnTick(&m_stack_idle, &m_stack_active))
        {
            ScheduleContextSwitch();
        }
    }

    __stk_forceinline void EnterCriticalSection()
    {
        if (m_csu_nesting == 0)
            STK_RISCV_CRITICAL_SECTION_START(m_csu);

        ++m_csu_nesting;
    }

    __stk_forceinline void ExitCriticalSection()
    {
        STK_ASSERT(m_csu_nesting != 0);

        --m_csu_nesting;

        if (m_csu_nesting == 0)
            STK_RISCV_CRITICAL_SECTION_END(m_csu);
    }

    typedef IPlatform::IEventOverrider           eovrd_t;
    typedef PlatformRiscV::ISpecificEventHandler sehndl_t;

    Stack     m_stack_main;  //!< stack info
    jmp_buf   m_exit_buf;    //!< saved context of the exit point
    size_t    m_csu;         //!< user critical session
    uint32_t  m_csu_nesting; //!< depth of user critical session nesting
    eovrd_t  *m_overrider;   //!< platform events overrider
    sehndl_t *m_specific;    //!< platform-specific event handler
    int32_t   m_tick_period; //!< system tick periodicity (microseconds, ticks)
    bool      m_starting;    //!< 'true' when in is being started
    bool      m_started;     //!< 'true' when in started state
    bool      m_exiting;     //!< 'true' when is exiting the scheduling process
}
* g_Context[_STK_ARCH_CPU_COUNT] = {};

void PlatformRiscV::ProcessTick()
{
#ifdef _STK_RISCV_USE_PENDSV
    size_t cs;
    STK_RISCV_CRITICAL_SECTION_START(cs);

    GetContext()->OnTick();

    STK_RISCV_CRITICAL_SECTION_END(cs);
#else
    // unsupported scenario
    STK_ASSERT(false);
#endif
}

static __stk_forceinline void SaveContext()
{
    __asm volatile(
    // allocate stack memory for registers
    "addi sp, sp, -" REGSIZE       " \n"

    // save regs
    SREG " x1, 2*" REGBYTES "(sp)    \n"
    //SREG " x2, 3*" REGBYTES "(sp)  \n"  // skip saving sp, Stack pointer
    //SREG " x3, 4*" REGBYTES "(sp)  \n"  // skip saving gp, Global pointer (note: slot is used by fsr)
    SREG " x4, 5*" REGBYTES "(sp)    \n"
    SREG " x5, 6*" REGBYTES "(sp)    \n"
    SREG " x6, 7*" REGBYTES "(sp)    \n"
    SREG " x7, 8*" REGBYTES "(sp)    \n"
    SREG " x8, 9*" REGBYTES "(sp)    \n"
    SREG " x9, 10*" REGBYTES "(sp)   \n"
    SREG " x10, 11*" REGBYTES "(sp)  \n"
    SREG " x11, 12*" REGBYTES "(sp)  \n"
    SREG " x12, 13*" REGBYTES "(sp)  \n"
    SREG " x13, 14*" REGBYTES "(sp)  \n"
    SREG " x14, 15*" REGBYTES "(sp)  \n"
    SREG " x15, 16*" REGBYTES "(sp)  \n"
#if (__riscv_32e != 1)
    SREG " x16, 17*" REGBYTES "(sp)  \n"
    SREG " x17, 18*" REGBYTES "(sp)  \n"
    SREG " x18, 19*" REGBYTES "(sp)  \n"
    SREG " x19, 20*" REGBYTES "(sp)  \n"
    SREG " x20, 21*" REGBYTES "(sp)  \n"
    SREG " x21, 22*" REGBYTES "(sp)  \n"
    SREG " x22, 23*" REGBYTES "(sp)  \n"
    SREG " x23, 24*" REGBYTES "(sp)  \n"
    SREG " x24, 25*" REGBYTES "(sp)  \n"
    SREG " x25, 26*" REGBYTES "(sp)  \n"
    SREG " x26, 27*" REGBYTES "(sp)  \n"
    SREG " x27, 28*" REGBYTES "(sp)  \n"
    SREG " x28, 29*" REGBYTES "(sp)  \n"
    SREG " x29, 30*" REGBYTES "(sp)  \n"
    SREG " x30, 31*" REGBYTES "(sp)  \n"
    SREG " x31, 32*" REGBYTES "(sp)  \n"
#endif

#if (__riscv_flen != 0)
    // save FP
    FSREG " f0, " FOFFSET "+0*" FREGBYTES "(sp)  \n"
    FSREG " f1, " FOFFSET "+1*" FREGBYTES "(sp)  \n"
    FSREG " f2, " FOFFSET "+2*" FREGBYTES "(sp)  \n"
    FSREG " f3, " FOFFSET "+3*" FREGBYTES "(sp)  \n"
    FSREG " f4, " FOFFSET "+4*" FREGBYTES "(sp)  \n"
    FSREG " f5, " FOFFSET "+5*" FREGBYTES "(sp)  \n"
    FSREG " f6, " FOFFSET "+6*" FREGBYTES "(sp)  \n"
    FSREG " f7, " FOFFSET "+7*" FREGBYTES "(sp)  \n"
    FSREG " f8, " FOFFSET "+8*" FREGBYTES "(sp)  \n"
    FSREG " f9, " FOFFSET "+9*" FREGBYTES "(sp)  \n"
    FSREG " f10, " FOFFSET "+10*" FREGBYTES "(sp)  \n"
    FSREG " f11, " FOFFSET "+11*" FREGBYTES "(sp)  \n"
    FSREG " f12, " FOFFSET "+11*" FREGBYTES "(sp)  \n"
    FSREG " f13, " FOFFSET "+12*" FREGBYTES "(sp)  \n"
    FSREG " f14, " FOFFSET "+13*" FREGBYTES "(sp)  \n"
    FSREG " f15, " FOFFSET "+14*" FREGBYTES "(sp)  \n"
    FSREG " f16, " FOFFSET "+15*" FREGBYTES "(sp)  \n"
    FSREG " f17, " FOFFSET "+16*" FREGBYTES "(sp)  \n"
    FSREG " f18, " FOFFSET "+17*" FREGBYTES "(sp)  \n"
    FSREG " f19, " FOFFSET "+18*" FREGBYTES "(sp)  \n"
    FSREG " f20, " FOFFSET "+19*" FREGBYTES "(sp)  \n"
    FSREG " f21, " FOFFSET "+20*" FREGBYTES "(sp)  \n"
    FSREG " f22, " FOFFSET "+21*" FREGBYTES "(sp)  \n"
    FSREG " f23, " FOFFSET "+22*" FREGBYTES "(sp)  \n"
    FSREG " f24, " FOFFSET "+23*" FREGBYTES "(sp)  \n"
    FSREG " f25, " FOFFSET "+24*" FREGBYTES "(sp)  \n"
    FSREG " f26, " FOFFSET "+25*" FREGBYTES "(sp)  \n"
    FSREG " f27, " FOFFSET "+26*" FREGBYTES "(sp)  \n"
    FSREG " f28, " FOFFSET "+27*" FREGBYTES "(sp)  \n"
    FSREG " f29, " FOFFSET "+28*" FREGBYTES "(sp)  \n"
    FSREG " f30, " FOFFSET "+29*" FREGBYTES "(sp)  \n"
    FSREG " f31, " FOFFSET "+30*" FREGBYTES "(sp)  \n"
#endif

    // save PC, Status of the current process
    "csrr t0, mepc                   \n"
    "csrr t1, mstatus                \n"
    SREG " t0, 0*" REGBYTES "(sp)    \n"
    SREG " t1, 1*" REGBYTES "(sp)    \n"

#if (__riscv_flen != 0)
    // save FCSR
    "frcsr t0                        \n"
    SREG " t0, 4*" REGBYTES "(sp)    \n"  // use stack memory slot of gp  (see comment for x3 above)
#endif
    );

    // save SP of the idle stack
    __asm volatile(
    LREG " t0, %0                    \n"
    SREG " sp, 0(t0)                 \n"
    : /* output: none */
#ifdef _STK_RISCV_USE_PENDSV
    : "m"(GetContext()->m_stack_idle)
#else
    : "m"(GetContext()->m_stack_active)
#endif
    : /* clobbers: none */);
}

static __stk_forceinline void LoadContext()
{
    // load SP of the active stack
    __asm volatile(
    LREG " t0, %0                    \n"
    LREG " sp, 0(t0)                 \n" // load
    : /* output: none */
    : "m"(GetContext()->m_stack_active)
    : /* clobbers: none */);

    __asm volatile(
    // load PC, Status
    LREG " t0, 0*" REGBYTES "(sp)    \n"
    LREG " t1, 1*" REGBYTES "(sp)    \n"
    "csrw mepc, t0                   \n"
    "csrw mstatus, t1                \n"

#if (__riscv_flen != 0)
    // load FCSR
    LREG " t0, 4*" REGBYTES "(sp)    \n" // use stack memory slot of gp (see comment for x3 below)
    "fscsr t0                        \n"
#endif

    // load regs
    LREG " x1, 2*" REGBYTES "(sp)    \n"
    //LREG " x2, 3*" REGBYTES "(sp)  \n" // skip loading sp, Stack pointer
    //LREG " x3, 4*" REGBYTES "(sp)  \n" // skip loading gp, Global pointer (note: slot is used by fsr)
    LREG " x4, 5*" REGBYTES "(sp)    \n"
    LREG " x5, 6*" REGBYTES "(sp)    \n"
    LREG " x6, 7*" REGBYTES "(sp)    \n"
    LREG " x7, 8*" REGBYTES "(sp)    \n"
    LREG " x8, 9*" REGBYTES "(sp)    \n"
    LREG " x9, 10*" REGBYTES "(sp)   \n"
    LREG " x10, 11*" REGBYTES "(sp)  \n"
    LREG " x11, 12*" REGBYTES "(sp)  \n"
    LREG " x12, 13*" REGBYTES "(sp)  \n"
    LREG " x13, 14*" REGBYTES "(sp)  \n"
    LREG " x14, 15*" REGBYTES "(sp)  \n"
    LREG " x15, 16*" REGBYTES "(sp)  \n"
#if (__riscv_32e != 1)
    LREG " x16, 17*" REGBYTES "(sp)  \n"
    LREG " x17, 18*" REGBYTES "(sp)  \n"
    LREG " x18, 19*" REGBYTES "(sp)  \n"
    LREG " x19, 20*" REGBYTES "(sp)  \n"
    LREG " x20, 21*" REGBYTES "(sp)  \n"
    LREG " x21, 22*" REGBYTES "(sp)  \n"
    LREG " x22, 23*" REGBYTES "(sp)  \n"
    LREG " x23, 24*" REGBYTES "(sp)  \n"
    LREG " x24, 25*" REGBYTES "(sp)  \n"
    LREG " x25, 26*" REGBYTES "(sp)  \n"
    LREG " x26, 27*" REGBYTES "(sp)  \n"
    LREG " x27, 28*" REGBYTES "(sp)  \n"
    LREG " x28, 29*" REGBYTES "(sp)  \n"
    LREG " x29, 30*" REGBYTES "(sp)  \n"
    LREG " x30, 31*" REGBYTES "(sp)  \n"
    LREG " x31, 32*" REGBYTES "(sp)  \n"
#endif

#if (__riscv_flen != 0)
    // load FP
    FLREG " f0, " FOFFSET "+0*" FREGBYTES "(sp)  \n"
    FLREG " f1, " FOFFSET "+1*" FREGBYTES "(sp)  \n"
    FLREG " f2, " FOFFSET "+2*" FREGBYTES "(sp)  \n"
    FLREG " f3, " FOFFSET "+3*" FREGBYTES "(sp)  \n"
    FLREG " f4, " FOFFSET "+4*" FREGBYTES "(sp)  \n"
    FLREG " f5, " FOFFSET "+5*" FREGBYTES "(sp)  \n"
    FLREG " f6, " FOFFSET "+6*" FREGBYTES "(sp)  \n"
    FLREG " f7, " FOFFSET "+7*" FREGBYTES "(sp)  \n"
    FLREG " f8, " FOFFSET "+8*" FREGBYTES "(sp)  \n"
    FLREG " f9, " FOFFSET "+9*" FREGBYTES "(sp)  \n"
    FLREG " f10, " FOFFSET "+10*" FREGBYTES "(sp)  \n"
    FLREG " f11, " FOFFSET "+11*" FREGBYTES "(sp)  \n"
    FLREG " f12, " FOFFSET "+11*" FREGBYTES "(sp)  \n"
    FLREG " f13, " FOFFSET "+12*" FREGBYTES "(sp)  \n"
    FLREG " f14, " FOFFSET "+13*" FREGBYTES "(sp)  \n"
    FLREG " f15, " FOFFSET "+14*" FREGBYTES "(sp)  \n"
    FLREG " f16, " FOFFSET "+15*" FREGBYTES "(sp)  \n"
    FLREG " f17, " FOFFSET "+16*" FREGBYTES "(sp)  \n"
    FLREG " f18, " FOFFSET "+17*" FREGBYTES "(sp)  \n"
    FLREG " f19, " FOFFSET "+18*" FREGBYTES "(sp)  \n"
    FLREG " f20, " FOFFSET "+19*" FREGBYTES "(sp)  \n"
    FLREG " f21, " FOFFSET "+20*" FREGBYTES "(sp)  \n"
    FLREG " f22, " FOFFSET "+21*" FREGBYTES "(sp)  \n"
    FLREG " f23, " FOFFSET "+22*" FREGBYTES "(sp)  \n"
    FLREG " f24, " FOFFSET "+23*" FREGBYTES "(sp)  \n"
    FLREG " f25, " FOFFSET "+24*" FREGBYTES "(sp)  \n"
    FLREG " f26, " FOFFSET "+25*" FREGBYTES "(sp)  \n"
    FLREG " f27, " FOFFSET "+26*" FREGBYTES "(sp)  \n"
    FLREG " f28, " FOFFSET "+27*" FREGBYTES "(sp)  \n"
    FLREG " f29, " FOFFSET "+28*" FREGBYTES "(sp)  \n"
    FLREG " f30, " FOFFSET "+29*" FREGBYTES "(sp)  \n"
    FLREG " f31, " FOFFSET "+30*" FREGBYTES "(sp)  \n"
#endif

    // shrink stack memory of registers
    "addi sp, sp, " REGSIZE " \n");
}

static __stk_forceinline void EnableFullFpuAccess()
{
#if (__riscv_flen != 0)
    __asm volatile(
    "li t0, %0        \n"
    "csrs mstatus, t0 \n"
    ::
    "i"(MSTATUS_FS | MSTATUS_XS));
#endif
}

static __stk_forceinline void ClearFpuState()
{
#if (__riscv_flen != 0)
    __asm volatile(
    "fssr x0"
    : /* output: none */
    : /* input: none */
    : /* clobbers: none */);
#endif
}

static __stk_forceinline void SaveMainSP()
{
    __asm volatile(
    SREG " sp, %0"
    : "=m"(GetContext()->m_stack_main)
    : /* input: none */
    : /* clobbers: none */);
}

static __stk_forceinline void LoadMainSP()
{
    __asm volatile(
    LREG " sp, %0"
    : /* output: none */
    : "m"(GetContext()->m_stack_main)
    : /* clobbers: none */);
}

__stk_forceinline void OnTaskRun()
{
    LoadContext();

    STK_RISCV_EXIT_FROM_HANDLER();
}

extern "C" STK_RISCV_ISR_SECTION __stk_attr_used void TrySwitchContext() // __stk_attr_used for LTO
{
    // make sure SysTick is enabled by the Kernel::Start(), disable its start anywhere else
    STK_ASSERT(GetContext()->m_started);
    STK_ASSERT(GetContext()->m_handler != NULL);

    // reschedule timer (note: before OnTick because timer can be stopped in Stop)
    HW_SetMtimecmp(GetContext()->m_tick_period);

    // process tick
    GetContext()->OnTick();
}

#ifdef _STK_RISCV_USE_PENDSV
STK_RISCV_ISR void _STK_SYSTICK_HANDLER()
{
    // save SP before switching to the main
    size_t sp = HW_GetCallerSP();

    // load SP of the main stack to handle ISR
    LoadMainSP();

    // try switch context (do via asm function call to avoid inlining)
    __asm volatile(
    "jal ra, TrySwitchContext"
    : /* output: none */
    : /* input: none */
    : /* clobbers: none */);

    // restore SP
    __asm volatile(
    LREG " sp, %0"
    : /* output: none */
    : "m"(sp)
    : /* clobbers: none */);
}
#else
extern "C" STK_RISCV_ISR_SECTION __stk_attr_naked void _STK_SYSTICK_HANDLER()
{
    // save current context (unconditionally)
    SaveContext();

    // internal ISR processing
    {
        // load SP of the main stack to handle ISR
        LoadMainSP();

        // try switch context (do via asm function call to avoid inlining)
        __asm volatile(
        "jal ra, TrySwitchContext"
        : /* output: none */
        : /* input: none */
        : /* clobbers: none */);
    }

    // re-load context to restore trashed process's register values by the internal ISRs processing
    LoadContext();

    STK_RISCV_EXIT_FROM_HANDLER();
}
#endif

static __stk_forceinline void StartScheduling()
{
#ifdef STK_RISCV_USE_MAIN_STACK_FOR_ISR
    // save SP of main stack to reuse it for subsequent ISRs
    SaveMainSP();
#endif

    // enable FPU (if available)
    EnableFullFpuAccess();

    // clear FPU usage status if FPU was used before kernel start
    ClearFpuState();

    // notify kernel
    GetContext()->m_handler->OnStart(&GetContext()->m_stack_active);

    // configure timer
    HW_SetMtimecmp(GetContext()->m_tick_period);

    // change state before enabling interrupt
    GetContext()->m_started  = true;
    GetContext()->m_starting = false;

    // enable timer interrupt
    set_csr(mie, MIP_MTIP);
}

STK_RISCV_ISR void _STK_SVC_HANDLER()
{
    size_t cause;
    __asm volatile("csrr %0, mcause"
    : "=r"(cause)
    : /* input : none */
    : /* clobbers: none */);

    /*if (cause & (1UL << (__riscv_xlen - 1)))
    {
        cause &= ~(1UL << (__riscv_xlen - 1));

        if (cause == IRQ_M_TIMER)
        {

        }
    }*/

    if (cause == IRQ_M_EXT)
    {
        // not starting scheduler, then try to forward ecall to user
        if (!GetContext()->m_starting)
        {
            // forward event to user
            if (GetContext()->m_specific != NULL)
                GetContext()->m_specific->OnException(cause);

            // switch to the next instruction of the caller space (PC) after the return
            write_csr(mepc, read_csr(mepc) + sizeof(size_t));
        }
        else
        {
            // schedule first task
            StartScheduling();
            OnTaskRun();
        }
    }
    else
    {
        if ((GetContext() != NULL) && (GetContext()->m_specific != NULL))
        {
            // forward event to user
            GetContext()->m_specific->OnException(cause);
        }
        else
        {
            // trap further execution
            for (;;)
            {
                //assert(false);
                STK_RISCV_WFI();
            }
        }
    }
}

static void OnTaskExit()
{
    size_t cs;
    STK_RISCV_CRITICAL_SECTION_START(cs);

    GetContext()->m_handler->OnTaskExit(GetContext()->m_stack_active);

    STK_RISCV_CRITICAL_SECTION_END(cs);

    for (;;)
    {
        STK_RISCV_WFI(); // enter standby mode until time slot expires
    }
}

static STK_RISCV_ISR_SECTION void OnSchedulerSleep()
{
#if STK_SEGGER_SYSVIEW
    SEGGER_SYSVIEW_OnIdle();
#endif

    for (;;)
    {
        __asm volatile ("fence rw, rw" : : : "memory"); // data barrier (DSB)
        STK_RISCV_WFI();                                // enter sleep until interrupt
        __asm volatile ("fence.i" : : : "memory");      // instruction sync (ISB)
    }
}

static STK_RISCV_ISR_SECTION void OnSchedulerSleepOverride()
{
    if (!GetContext()->m_overrider->OnSleep())
        OnSchedulerSleep();
}

static void OnSchedulerExit()
{
    LoadMainSP(); // switch to main stack

    // jump to the exit from the IKernel::Start()
    longjmp(GetContext()->m_exit_buf, 0);
}

void PlatformRiscV::Initialize(const IMemory &ctx_memory, IEventHandler *event_handler, uint32_t resolution_us,
    Stack *exit_trap)
{
    // note: if fails, increase the memory size
    STK_ASSERT(ctx_memory.GetSizeBytes() >= sizeof(Context));

    // allocate context within its memory region
    SetContext(new (ctx_memory.GetPtr()) Context());

    GetContext()->Initialize(event_handler, exit_trap, resolution_us);
}

void PlatformRiscV::Start()
{
    GetContext()->m_exiting = false;

    // save jump location of the Exit trap
    setjmp(GetContext()->m_exit_buf);
    if (GetContext()->m_exiting)
        return;

    // enable FPU (if available)
    EnableFullFpuAccess();

    // start
    GetContext()->m_starting = true;
    STK_RISCV_START_SCHEDULING();
}

bool PlatformRiscV::InitStack(EStackType stack_type, Stack *stack, IStackMemory *stack_memory, ITask *user_task)
{
    STK_ASSERT(stack_memory->GetStackSize() > (STK_RISCV_REGISTER_COUNT + STK_SERVICE_SLOTS));

    // initialize stack memory
    size_t *stack_top = PlatformContext::InitStackMemory(stack_memory);

    // initialize Stack Pointer (SP)
    stack->SP = (size_t)(stack_top - (STK_RISCV_REGISTER_COUNT + STK_SERVICE_SLOTS));

    size_t MEPC, RA, X10;
    size_t MSTATUS = MSTATUS_MPP | MSTATUS_MPIE | (STK_RISCV_FP != 0 ? (MSTATUS_FS | MSTATUS_XS) : 0);
#if (STK_RISCV_FP != 0)
    size_t FSR = 0;
#endif

    // initialize registers for the user task's first start
    switch (stack_type)
    {
    case STACK_USER_TASK: {
        MEPC = (size_t)user_task->GetFunc();
        RA   = (size_t)OnTaskExit;
        X10  = (size_t)user_task->GetFuncUserData();
        break; }

    case STACK_SLEEP_TRAP: {
        MEPC = (size_t)(GetContext()->m_overrider != NULL ? OnSchedulerSleepOverride : OnSchedulerSleep);
        RA   = (size_t)STK_STACK_MEMORY_FILLER; // should not attempt to exit
        X10  = 0;
        break; }

    case STACK_EXIT_TRAP: {
        MEPC = (size_t)OnSchedulerExit;
        RA   = (size_t)STK_STACK_MEMORY_FILLER; // should not attempt to exit
        X10  = 0;
        break; }

    default:
        return false;
    }

    stack_top[STK_RISCV_SRV_INDEX(1)]  = MEPC;    // mepc (entry function)
    stack_top[STK_RISCV_SRV_INDEX(2)]  = MSTATUS; // mstatus (entry function)

    stack_top[STK_RISCV_REG_INDEX(1)]  = RA;      // x1, ra
#if (STK_RISCV_FP != 0)
    stack_top[STK_RISCV_REG_INDEX(3)]  = FSR;     // x3, fssr (note: x4 is gp register but we use this slot to hold value for fsr register)
#endif
    stack_top[STK_RISCV_REG_INDEX(10)] = X10;     // x10, function argument

    return true;
}

static void SysTick_Stop()
{
    clear_csr(mie, MIP_MTIP);
}

void PlatformRiscV::Stop()
{
    // stop timer
    SysTick_Stop();

    GetContext()->m_started = false;
    GetContext()->m_exiting = true;

    // make sure all assignments are set and executed
    __sync_synchronize();
}

int32_t PlatformRiscV::GetTickResolution() const
{
    return GetContext()->m_tick_resolution;
}

void PlatformRiscV::SwitchToNext()
{
    GetContext()->m_handler->OnTaskSwitch(::HW_GetCallerSP());
}

void PlatformRiscV::SleepTicks(uint32_t ticks)
{
    GetContext()->m_handler->OnTaskSleep(::HW_GetCallerSP(), ticks);
}

void PlatformRiscV::ProcessHardFault()
{
    if ((GetContext()->m_overrider == NULL) || !GetContext()->m_overrider->OnHardFault())
    {
        exit(1);
    }
}

void PlatformRiscV::SetEventOverrider(IEventOverrider *overrider)
{
    STK_ASSERT(!GetContext()->m_started);
    GetContext()->m_overrider = overrider;
}

size_t PlatformRiscV::GetCallerSP()
{
    return ::HW_GetCallerSP();
}

void PlatformRiscV::SetSpecificEventHandler(ISpecificEventHandler *handler)
{
    STK_ASSERT(!GetContext()->m_started);
    GetContext()->m_specific = handler;
}

void stk::EnterCriticalSection()
{
    GetContext()->EnterCriticalSection();
}

void stk::ExitCriticalSection()
{
    GetContext()->ExitCriticalSection();
}

#endif // _STK_ARCH_RISC_V
