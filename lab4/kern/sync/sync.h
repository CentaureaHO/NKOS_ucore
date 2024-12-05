//一个同步机制相关的头文件，定义了一些用于中断保存和恢复的宏和内联函数
#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

static inline bool __intr_save(void) {//用于保存中断状态并禁用中断
    if (read_csr(sstatus) & SSTATUS_SIE) {//首先检查当前的 sstatus 寄存器的值是否设置了 SIE（中断使能）位
        intr_disable();
        return 1;
    }//如果是，则禁用中断，并返回1，表示中断被保存
    return 0;
}//否则，返回0，表示中断未保存。

static inline void __intr_restore(bool flag) {//用于恢复中断状态
    if (flag) {//如果之前的中断被保存了（即传入的 flag 为真），则调用 intr_enable 函数重新启用中断。
        intr_enable();
    }
}

//local_intr_save 宏用于保存中断状态，并将保存的结果存储在给定的变量 x 中。
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
//local_intr_restore 宏用于恢复中断状态。
#endif /* !__KERN_SYNC_SYNC_H__ */