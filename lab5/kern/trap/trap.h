#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

struct pushregs
{
    uintptr_t zero;  // 硬编码零
    uintptr_t ra;    // 返回地址
    uintptr_t sp;    // 栈指针
    uintptr_t gp;    // 全局指针
    uintptr_t tp;    // 线程指针
    uintptr_t t0;    // 临时寄存器
    uintptr_t t1;    // 临时寄存器
    uintptr_t t2;    // 临时寄存器
    uintptr_t s0;    // 保存的寄存器/帧指针
    uintptr_t s1;    // 保存的寄存器
    uintptr_t a0;    // 函数参数/返回值
    uintptr_t a1;    // 函数参数/返回值
    uintptr_t a2;    // 函数参数
    uintptr_t a3;    // 函数参数
    uintptr_t a4;    // 函数参数
    uintptr_t a5;    // 函数参数
    uintptr_t a6;    // 函数参数
    uintptr_t a7;    // 函数参数
    uintptr_t s2;    // 保存的寄存器
    uintptr_t s3;    // 保存的寄存器
    uintptr_t s4;    // 保存的寄存器
    uintptr_t s5;    // 保存的寄存器
    uintptr_t s6;    // 保存的寄存器
    uintptr_t s7;    // 保存的寄存器
    uintptr_t s8;    // 保存的寄存器
    uintptr_t s9;    // 保存的寄存器
    uintptr_t s10;   // 保存的寄存器
    uintptr_t s11;   // 保存的寄存器
    uintptr_t t3;    // 临时寄存器
    uintptr_t t4;    // 临时寄存器
    uintptr_t t5;    // 临时寄存器
    uintptr_t t6;    // 临时寄存器
};

struct trapframe
{
    struct pushregs gpr;
    uintptr_t       status;
    uintptr_t       epc;
    uintptr_t       tval;
    uintptr_t       cause;
};

void trap(struct trapframe* tf);
void idt_init(void);
void print_trapframe(struct trapframe* tf);
void print_regs(struct pushregs* gpr);
bool trap_in_kernel(struct trapframe* tf);

#endif /* !__KERN_TRAP_TRAP_H__ */
