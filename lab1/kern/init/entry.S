#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    la sp, bootstacktop

    tail kern_init      # 尾调用不保存返回地址，不额外使用栈空间
                        # 其实和j kern_init差不多

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop    # 位于.data段，分配KSTACKSIZE大小的空间
                            # KSATCKSIZE声明位于memlayout.h，实际使用8KB
                            # 划分规则见kernel.ld
bootstacktop: