#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>
#include <sbi.h>

//__asm__ ("ebreak");
//__asm__ ("mret");
#include <sbi.h>

//__asm__ ("ebreak");
//__asm__ ("mret");

#define TICK_NUM 100
volatile size_t num         = 0;
volatile size_t tick_counts = 0;

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/**
 * @brief      Load supervisor trap entry in RISC-V
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe* tf) { return (tf->status & SSTATUS_SPP) != 0; }

void print_trapframe(struct trapframe* tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs* gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

void interrupt_handler(struct trapframe* tf)
{
    intptr_t cause = (tf->cause << 1) >> 1;  // 抹掉scause最高位代表“这是中断不是异常”的1
    switch (cause)
    {
        case IRQ_U_SOFT: cprintf("User software interrupt\n"); break;
        case IRQ_S_SOFT: cprintf("Supervisor software interrupt\n"); break;
        case IRQ_H_SOFT: cprintf("Hypervisor software interrupt\n"); break;
        case IRQ_M_SOFT: cprintf("Machine software interrupt\n"); break;
        case IRQ_U_TIMER: cprintf("User software interrupt\n"); break;
        case IRQ_S_TIMER:
            clock_set_next_event();
            ++tick_counts;
            if (tick_counts == TICK_NUM)
            {
                cprintf("100 ticks\n");
                ++num;
                tick_counts = 0;
                // if (num == 10) { sbi_shutdown(); }
            }
            break;
    }
}

void exception_handler(struct trapframe* tf)
{
    switch (tf->cause)
    {
        case CAUSE_MISALIGNED_FETCH: break;
        case CAUSE_FAULT_FETCH: break;
        case CAUSE_BREAKPOINT:
            cprintf("ebreak caught at 0x%x\n", tf->epc);
            cprintf("Exception type: breakpoint\n");
            tf->epc += 2;  // ebreak属于压缩指令集C，仅占用2字节
            break;
        case CAUSE_ILLEGAL_INSTRUCTION:
            cprintf("Illegal instruction caught at 0x%x\n", tf->epc);
            cprintf("Exception type: Illegal instruction\n");
            tf->epc += 4;
            break;
        case CAUSE_MISALIGNED_LOAD: break;
        case CAUSE_FAULT_LOAD: break;
        case CAUSE_MISALIGNED_STORE: break;
        case CAUSE_FAULT_STORE: break;
        case CAUSE_USER_ECALL: break;
        case CAUSE_SUPERVISOR_ECALL: break;
        case CAUSE_HYPERVISOR_ECALL: break;
        case CAUSE_MACHINE_ECALL: break;
        default:
            print_trapframe(tf);
            cprintf("ebreak caught at 0x%x\n", tf->epc);
            cprintf("Exception type: breakpoint\n");
            tf->epc += 2;
            break;
    }
}

/* trap_dispatch - dispatch based on what type of trap occurred */
/* trap_dispatch - dispatch based on what type of trap occurred */
static inline void trap_dispatch(struct trapframe* tf)
{
    if ((intptr_t)tf->cause < 0)
    {  // 如果scause的最高位是1，说明trap是由中断引起的
        // interrupts
        // cprintf("Interrupt\n");
        // cprintf("Interrupt\n");
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        // cprintf("Exception\n");
        // cprintf("Exception\n");
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe* tf) { trap_dispatch(tf); }
