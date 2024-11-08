#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <slub.h>
int  kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    cons_init();  // init the console
    const char* message = "(THU.CST) os is loading ...\0";
    // cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    idt_init();  // init interrupt descriptor table

    pmm_init();  // init physical memory management
                 // challenge2使用

    list_entry_t* node = slub_malloc(sizeof(list_entry_t));
    assert(node != NULL);
    slub_free(node);

    int* test_int = slub_malloc(sizeof(int));
    *test_int     = 114514;
    slub_free(test_int);
    int* test_int2 = slub_malloc(sizeof(int));
    *test_int2     = 1919810;
    cprintf("Malloc a int of %d\n", *test_int);
    // slub_free(test_int2);
    if (slub_free(test_int2) == 0)
        cprintf("Release int2\n");
    else
        cprintf("Fail to release int2\n");

    idt_init();  // init interrupt descriptor table

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    /* do nothing */
    while (1);
}

void __attribute__((noinline)) grade_backtrace2(int arg0, int arg1, int arg2, int arg3)
{
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1)
{
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) { grade_backtrace1(arg0, arg2); }

void grade_backtrace(void) { grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000); }

static void lab1_print_cur_status(void)
{
    static int round = 0;
    round++;
}
