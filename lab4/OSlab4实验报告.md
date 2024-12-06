# Lab4 :进程管理

小组成员： 2210878 唐显达  2213040 王禹衡   2210983  苑译元

## 练习0：填写已有实验

> 本实验依赖实验2/3。请把你做的实验2/3的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。
>

do_pgfault需要复制进来，其他的基本不需要复制Lab2和Lab3的代码。 



## 练习1：分配并初始化一个进程控制块（需要编码）

> alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。
>
> > 【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。
>
> 请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
> - 请说明proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）



 alloc_proc 函数的作⽤是实现初始化空闲进程的函数，其调⽤了 kmalloc 函数获得⼀个空的进 程控制块作为空闲进程的进程控制块，然后如果获取成功会对进程控制块这个结构体中的成员进⾏初 始化，会将进程状态设置成 PROC_UNINIT 代表进程初始态，pid设为-1代表进程还未分配，cr3设为 uCore内核⻚表的基址，剩余的成员⼏乎都是进⾏清零和置空处理，具体代码如下：

```c
// alloc_proc - 分配一个proc_struct并初始化proc_struct的所有字段
static struct proc_struct *alloc_proc(void) {
    // 使用kmalloc分配内存空间给新的proc_struct
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        // LAB4:EXERCISE1 你的代码
        /** 需要初始化的proc_struct中的字段 */
        proc->state = PROC_UNINIT;            // 设置进程状态为未初始化
        proc->pid = -1;                       // 设置进程ID为-1（还未分配）
        proc->cr3 = boot_cr3;                 // 设置CR3寄存器的值（页目录基址）
        proc->runs = 0;                       // 设置进程运行次数为0
        proc->kstack = 0;                     // 设置内核栈地址为0（还未分配）
        proc->need_resched = 0;               // 设置不需要重新调度
        proc->parent = NULL;                  // 设置父进程为空
        proc->mm = NULL;                      // 设置内存管理字段为空
        memset(&(proc->context), 0, sizeof(struct context)); // 初始化上下文信息为0
        proc->tf = NULL;                      // 设置trapframe为空
        proc->flags = 0;                      // 设置进程标志为0
        memset(proc->name, 0, PROC_NAME_LEN); // 初始化进程名为0
    }
    return proc; // 返回新分配的进程控制块
}
```

在操作系统中， proc_struct 数据结构⽤于存储有关进程的各种信息。 struct context 和 struct trapframe 是 proc_struct 中的成员，它们的成员变量含义如下： 

1. struct context context ⽤于保存进程上下⽂，即进程被中断或切换出CPU时需要保存的 ⼏个关键的寄存器，如程序计数器（PC）、堆栈指针（SP）和其他寄存器。当操作系统决定恢复 ⼀个进程的执⾏时，会从这个结构体中恢复寄存器的状态，从⽽继续执⾏进程。在 ucore 中， context 结构通常在上下⽂切换函数 switch_to 中被使⽤。
2. struct trapframe *tf 指针指向⼀个 trapframe 结构，该结构包含了当进程进⼊内核模 式时（⽐如因为系统调⽤或硬件中断）需要保存的信息。 trapframe 保存了中断发⽣时的CPU状 态，包括所有的寄存器值和程序计数器（epc）。这使得操作系统可以准确地了解中断发⽣时进程 的状态，并且可以在处理完中断后恢复到之前的状态继续执⾏。在系统调⽤或中断处理的代码中经 常会⽤到这个结构。 在本实验中，这两个结构在进程调度和中断处理的作⽤是： • context 在不同进程之间进⾏上下⽂切换时保存和恢复进程状态。当调度器选择⼀个新的进程运 ⾏时，它会使⽤保存在 context 中的信息来设置CPU的状态，从⽽开始执⾏新进程。 • tf 在处理系统调⽤、异常或中断时使⽤。当进程从⽤⼾模式切换到内核模式时，⽤⼾模式下的状 态（如寄存器）会被保存在 trapframe 中。内核完成处理后，可以使⽤这些信息来恢复进程的状 态并继续⽤⼾模式下的执⾏。 这两个结构对于内核能够管理进程的执⾏，特别是在处理中断和系统调⽤、实现多任务处理⽅⾯⾄关 重要。在设计和实现 alloc_proc 函数时，确保这些成员变量正确初始化是⾮常重要的，因为任何 错误的状态都可能导致进程⽆法正确运⾏或系统崩溃。

## 练习2：为新创建的内核线程分配资源（需要编码）

> 创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们**实际需要"fork"的东西就是stack和trapframe**。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：
>
> - 调用alloc_proc，首先获得一块用户信息块。
> - 为进程分配一个内核栈。
> - 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
> - 复制原进程上下文到新进程
> - 将新进程添加到进程列表
> - 唤醒新进程
> - 返回新进程号
>
> 请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
> - 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。





do_fork 函数是创建新进程的核⼼函数。在UNIX-like系统中， fork ⽤来创建⼀个与当前进程⼏乎 完全相同的⼦进程，实现的代码如下：



```c++
/*
 * do_fork - 通过父进程来创建一个新的子进程
 * @clone_flags: 用于指导如何克隆子进程
 * @stack: 父进程的用户栈指针。如果stack为0，则意味着要fork一个内核线程。
 * @tf: trapframe信息，它将被复制到子进程的proc->tf中
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC; // 默认返回值，表示没有空闲的进程结构体
    struct proc_struct *proc;

    // 检查当前进程数是否已达到系统限制
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM; // 如果内存不足，则返回内存错误码

    // 1. 调用alloc_proc来分配一个proc_struct
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }

    // 2. 调用setup_kstack为子进程分配内核栈
    proc->parent = current; // 设置子进程的父进程为当前进程
    if (setup_kstack(proc)) {
        goto bad_fork_cleanup_kstack;
    }

    // 3. 调用copy_mm根据clone_flag来复制或共享内存
    if (copy_mm(clone_flags, proc)) {
        goto bad_fork_cleanup_proc;
    }

    // 4. 调用copy_thread来设置子进程的tf和context
    copy_thread(proc, stack, tf);

    // 5. 将新进程添加到进程列表和哈希表中
    bool intr_flag;
    local_intr_save(intr_flag); // 禁用中断
    {
        proc->pid = get_pid(); // 为子进程分配一个唯一的进程ID
        hash_proc(proc); // 将新进程添加到哈希表中
        list_add(&proc_list, &(proc->list_link)); // 将新进程添加到进程列表中
    }
    local_intr_restore(intr_flag); // 恢复中断

    // 6. 调用wakeup_proc使新的子进程变为可运行状态
    wakeup_proc(proc);

    // 7. 使用子进程的pid作为返回值
    ret = proc->pid;

fork_out:
    return ret; // 返回子进程的PID或者错误码

bad_fork_cleanup_kstack:
    put_kstack(proc); // 如果内核栈设置失败，清理分配的内核栈
bad_fork_cleanup_proc:
    kfree(proc); // 清理分配的proc_struct
    goto fork_out;
}
```



do_fork 函数负责克隆当前进程的状态并⽣成⼀个新进程。该函数的执⾏始于调⽤ alloc_proc ，它构建了⼀个新的进程控制块作为线程的数据核⼼。我随后为新线程分配了内核 栈，确保它有⾃⼰的运⾏空间。由于我们创建的是内核线程，不涉及⽤⼾空间，因此我们跳过了内存 管理信息的复制。 

接下来的步骤是复制执⾏上下⽂，这通过 copy_thread 完成，它在新线程的内核栈中设置了必要的 初始状态。然后，新线程被加⼊到全局的进程列表中，这⼀步是在中断禁⽤的情况下进⾏的，以避免 并发操作的冲突。通过调⽤ wakeup_proc ，新线程被标记为可运⾏状态。 

最后，新线程的唯⼀进程标识符（PID）被分配并返回。这个PID是通过 get_pid 函数获取的，它保 证了每个线程的PID的唯⼀性，因为 get_pid 会遍历现有的进程列表以避免ID冲突。

###  关于 ucore 是否为每个新fork的线程分配⼀个唯⼀的PID 

ucore 确实能够为每个新fork的线程分配⼀个唯⼀的PID。通过 get_pid 函数的逻辑，它遍历当前 所有进程，以确保新分配的PID不与任何现有进程的PID冲突。在 get_pid 函数中，"安全的 PID"（ next_safe ）是指在 get_pid 函数中⽤来追踪可以安全分配⽽不会与现有进程的PID冲突 的PID值，也就是说，当前分配了 last_pid 这⼀PID后，下⼀次 last_pid+1 到 next_safe-1 的PID都是可以分配的。这个机制主要⽤于优化搜索性能，避免每次分配PID时都需要 遍历整个进程列表来检查PID是否已被占⽤。 也就是说：
• last_pid ：记录最后⼀次成功分配的PID。每次 get_pid 被调⽤时，它会从 last_pid + 1 开始寻找下⼀个可⽤的PID。

 • next_safe ：记录下⼀个没有被任何活动进程使⽤的PID。当 last_pid 达到或超过 next_safe 时， get_pid 函数知道它需要重新扫描活动进程列表来更新 next_safe 的值。 

这种⽅法可以减少每次寻找新PID时遍历进程列表的次数。例如，在⼀个PID范围⼤⽽活动进程相对少的系统中，这种优化可以显著提⾼PID分配的效率。如果 last_pid 加⼀后还没有到达 next_safe ， get_pid 就可以安全地分配 last_pid ⽽⽆需遍历进程列表。只有在 last_pid 达到 next_safe 时，才需要检查进程列表来更新 next_safe 。

 这个策略假定分配的PID会⽐较平均地分散在整个可⽤范围内，⽽且系统中的进程创建和销毁也是⽐较 均匀的。在这些假设下，“安全的PID”（ next_safe ）充当了⼀个缓存的⻆⾊，指向⼀个确定没 有被占⽤的PID，直到下⼀次必须重新扫描进程列表来更新这个信息为⽌。 基于 get_pid 函数的实现，只要 MAX_PID ⼤于 MAX_PROCESS 并且系统中的其他部分也正确地 管理着PID的分配和回收， ucore 就能为每个新fork的线程提供⼀个唯⼀的PID。这个设计假定系统 不会在极短时间内创建超过 MAX_PID 数量的进程，这在实践中是合理的。 

此外， local_intr_save(intr_flag); 和 local_intr_restore(intr_flag); 在分配 PID的过程中禁⽤和恢复中断，这保证了 get_pid 函数在寻找和分配新PID时不会被中断，从⽽避免 了并发执⾏的问题。且在 ucore 的实验设置中，操作系统运⾏在单核环境中，因此， do_fork 函 数在分配PID时不会遇到来⾃其他CPU核⼼的并发执⾏问题，也就是说保证了 get_pid 的原⼦性



## 练习3：编写proc_run 函数（需要编码）

> proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：
>
> - 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
> - 禁用中断。你可以使用`/kern/sync/sync.h`中定义好的宏`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。
> - 切换当前进程为要运行的进程。
> - 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了`lcr3(unsigned int cr3)`函数，可实现修改CR3寄存器值的功能。
> - 实现上下文切换。`/kern/process`中已经预先编写好了`switch.S`，其中定义了`switch_to()`函数。可实现两个进程的context切换。
> - 允许中断。
>
> 请回答如下问题：
>
> - 在本实验的执行过程中，创建且运行了几个内核线程？
>
> 完成代码编写后，编译并运行代码：make qemu
>
> 如果可以得到如 附录A所示的显示内容（仅供参考，不是标准答案输出），则基本正确。



proc_run 的实现如下： 

```c
// proc_run - 使得进程 "proc" 在CPU上运行
// 注意：在调用switch_to之前，应当加载"proc"的新页目录表(PDT)的基地址
void proc_run(struct proc_struct *proc)
{
    // 只有当proc不是当前进程时才需要进行上下文切换
    if (proc != current)
    {
        // LAB4:EXERCISE3 你的代码
        /*
         * 以下是一些有用的宏、函数和定义，你可以在下面的实现中使用它们。
         * 宏或函数:
         * local_intr_save(): 禁用中断
         * local_intr_restore(): 启用中断
         * lcr3(): 修改CR3寄存器的值，用于切换当前使用的页目录表
         * switch_to(): 在两个进程之间进行上下文切换
         */

        // 定义用于保存中断状态的变量
        bool intr_flag;

        // 记录当前进程和即将运行的进程
        struct proc_struct *prev = current, *next = proc;

        // 禁用中断以保护上下文切换过程
        local_intr_save(intr_flag);
        {
            // 更新当前进程为proc
            current = proc;

            // 加载新进程的页目录表到CR3寄存器并切换地址空间
            lcr3(proc->cr3); // 注意：这里使用proc->cr3而不是next->cr3，因为next只是别名，且已在上面定义

            // 执行上下文切换，切换到新进程
            switch_to(&(prev->context), &(next->context));

            // 注意：switch_to之后，代码的执行流将不会返回到这里，
            // 因为switch_to已经通过某种机制（如硬件中断或长跳转）切换到了另一个进程的上下文。
            // 因此，下面的代码（如果有的话）将不会被执行。
        }

        // 恢复之前的中断状态（实际上，由于上面的注释，这行代码在逻辑上永远不会被执行）
        // 但为了保持代码的完整性和结构，我们还是保留它。
        // 在实际的操作系统内核中，可能会通过其他机制来确保中断的正确恢复。
        local_intr_restore(intr_flag);

        // 注意：在实际的内核代码中，由于上下文切换的特殊性，
        // 上面的local_intr_restore调用在逻辑上是不需要的，因为控制权已经转移到了另一个进程。
        // 但是，为了保持函数的完整性和清晰的错误处理逻辑，我们在这里保留了它。
        // 在真正的内核实现中，这部分代码可能需要被适当地优化或重构。
    }

    // 如果proc是当前进程，则不需要进行任何操作，因为已经在运行了。
    // 这部分逻辑是隐含在if条件判断之外的。
}
```

在本实验中，⼀共创建了两个内核线程，⼀个为 idle 另外⼀个为执⾏ init_main 的 init 线 程。这是因为在 proc_init 函数中创建了两个内核线程： 

1. idleproc: 这是第⼀个内核线程，称为idle线程，它的作⽤是在没有其他线程可运⾏时占据CPU。这 个线程通常会运⾏⼀个简单的循环，等待其他线程变为可运⾏状态。

2. initproc: 这是第⼆个内核线程，由 kernel_thread(init_main, "Hello world!!", 0) 创建，这个线程开始执⾏ init_main 函数，也就是打印消息：



```c
#include <stdio.h> // 假设 cprintf 类似于 printf，但可能需要特定的内核头文件

// 假设这些宏或函数在内核的其他部分被定义
extern struct proc_struct *current; // 当前进程的指针
extern const char* get_proc_name(struct proc_struct* proc); // 获取进程名称的函数

static int init_main(void *arg) {
    // 输出当前进程的 PID 和名称
    cprintf("This is initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    
    // 输出通过参数传递的字符串
    cprintf("To U: \"%s\"\n", (const char *)arg);
    
    // 输出额外的信息
    cprintf("To U: \"en..., Bye, Bye. :)\"\n");
    
    return 0; // 返回0通常表示成功
}
```

注意，这次实验 init_main 函数中是没有启动user_main内核线程的。 然后，在 cpu_idle 函数中会调⽤ schedule 调度函数，使可⽤的内核线程运⾏

## 扩展练习：Challenge

> 说明语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？





在操作实系现统开中关，中特断别的是在内核级别的代码中，经常需要临时禁⽤中断，以保护代码执⾏期间的临界区 不被中断，从⽽避免竞态条件和保持数据⼀致性；就是说，起到了⼀个形成临界区的作⽤， local_intr_save(intr_flag); 和 local_intr_restore(intr_flag); 就是这样的⼀ 对宏（内联函数），⽤于实现这个⽬的。它们的⼯作原理为：

1. local_intr_save(intr_flag); 这个语句保存当前的中断状态，并禁⽤当前CPU的本地中 断。 intr_flag 是⼀个局部布尔变量，⽤于保存旧的中断状态，以便稍后可以恢复。在RISC-V 或者其他体系结构上，这通常涉及读取中断控制寄存器的当前值（如状态寄存器），并将该值存储 在变量中。然后，先判断⼀下S态的中断使能位是否被设置了，如果设置了，则调⽤ intr_disable 函数设置中断控制寄存器的值以禁⽤中断。在RISC-V中，这可能涉及修改状态寄 存器（ sstatus ）中的全局中断使能位（SIE）
2.   禁⽤中断后，执⾏的代码块（即 local_intr_save 和 local_intr_restore 之间的代码） 可以安全地执⾏，不会被中断打断。 local_intr_restore(intr_flag); 这个语句恢复之 前通过 local_intr_save 保存的中断状态。它读取 intr_flag 变量，将中断状态寄存器设 置回之前保存的值。如果先前的状态是允许中断的，这将重新启⽤中断。这样做保证了如果在进⼊ 临界区之前中断是启⽤的，那么离开临界区后中断仍然是启⽤的。 local_intr_save(intr_flag); 会保存当前的中断状态到 intr_flag 并禁⽤中断（如果 它们当前是使能的）。 local_intr_restore(intr_flag); 会根据 intr_flag 的值恢复 之前的中断状态。如果 intr_flag 为1（true），中断会被重新使能

