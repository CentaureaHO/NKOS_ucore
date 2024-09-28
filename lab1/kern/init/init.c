#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void) {
    extern char edata[], end[];//声明了外部符号 edata 和 end，它们指向内存中某些特定位置，通常用于标识内核不同内存段的边界：
    //edata：已初始化的数据段（data segment）的结束地址。
    //end：未初始化的数据段（BSS segment）的结束地址。
    memset(edata, 0, end - edata);//用来清空BSS段（BSS段保存未初始化的全局变量），确保这些变量在程序开始执行时都是零。

    cons_init();  // init the console

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);

    print_kerninfo();//这一行调用 print_kerninfo 函数，输出内核的特定符号信息，比如内核入口、代码段结束、数据段结束等地址，帮助调试内核内存布局。

    // grade_backtrace();

    idt_init();  // init interrupt descriptor table
    //__asm__ __volatile__("ebreak");
    //__asm__ __volatile__("mret");

    // rdtime in mbare mode crashes
    clock_init();  // init clock interrupt

    intr_enable();  // enable irq interrupt
    
    while (1)
        ;
}

void __attribute__((noinline))
grade_backtrace2(unsigned long long arg0, unsigned long long arg1, unsigned long long arg2, unsigned long long arg3) {
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (unsigned long long)&arg0, arg1, (unsigned long long)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

void grade_backtrace(void) { grade_backtrace0(0, (unsigned long long)kern_init, 0xffff0000); }

static void lab1_print_cur_status(void) {
    static int round = 0;
    round++;
}



