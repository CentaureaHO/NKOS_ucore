#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- 进程/线程机制设计与实现 -------------
(一个简化版的 Linux 进程/线程机制)

介绍：
  ucore 实现了一个简单的进程/线程机制。进程包含独立的内存空间，至少有一个线程用于执行，内核数据（用于管理），处理器状态（用于上下文切换），文件（在实验6中），等等。ucore 需要有效地管理这些细节。在 ucore 中，线程只是进程的一种特殊类型（共享进程的内存）。

------------------------------
进程状态       :     含义                    -- 原因
    PROC_UNINIT     :   未初始化               -- alloc_proc
    PROC_SLEEPING   :   睡眠中                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   可运行（可能正在运行）  -- proc_init, wakeup_proc
    PROC_ZOMBIE     :   几乎死亡               -- do_exit

-----------------------------
进程状态变化：
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------

-----------------------------
进程关系
父进程：           proc->parent  (proc 为子进程)
子进程：           proc->cptr    (proc 为父进程)
年长的兄弟进程：   proc->optr    (proc 为年轻的兄弟进程)
年幼的兄弟进程：   proc->yptr    (proc 为年长的兄弟进程)

-----------------------------
与进程相关的系统调用：
SYS_exit        : 进程退出,                           -->do_exit
SYS_fork        : 创建子进程, 复制 mm                -->do_fork-->wakeup_proc
SYS_wait        : 等待进程                           -->do_wait
SYS_exec        : 在 fork 后，进程执行一个程序       -->加载程序并刷新 mm
SYS_clone       : 创建子线程                         -->do_fork-->wakeup_proc
SYS_yield       : 进程标记自己需要重新调度，         -- proc->need_sched=1, 然后调度器会重新调度该进程
SYS_sleep       : 进程睡眠                           -->do_sleep 
SYS_kill        : 终止进程                           -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : 获取进程的 pid

*/

// 进程集的列表
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - 分配一个 proc_struct 并初始化其所有字段
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
/*
 * 以下字段在 proc_struct 中需要初始化：
 *       enum proc_state state;                      // 进程状态
 *       int pid;                                    // 进程 ID
 *       int runs;                                   // 进程运行次数
 *       uintptr_t kstack;                           // 进程内核栈
 *       volatile bool need_resched;                 // 布尔值：是否需要重新调度以释放 CPU？
 *       struct proc_struct *parent;                 // 父进程
 *       struct mm_struct *mm;                       // 进程的内存管理字段
 *       struct context context;                     // 切换到此处以运行进程
 *       struct trapframe *tf;                       // 当前中断的陷阱帧
 *       uintptr_t cr3;                              // CR3 寄存器：页目录表（PDT）的基地址
 *       uint32_t flags;                             // 进程标志
 *       char name[PROC_NAME_LEN + 1];               // 进程名称
 */


        proc->state = PROC_UNINIT;  // 设置进程为“未初始化”状态——即第0个内核线程（空闲进程idleproc）
        proc->pid  = -1;  // 设置进程PID为未初始化值，即-1
        proc->runs = 0;   // 根据提示可知该成员变量表示进程的运行时间，初始化为0
        proc->kstack = 0;  // 进程内核栈初始化为空【kstack记录了分配给该进程/线程的内核栈的位置】
        proc->need_resched = 0;  // 是否需要重新调度以释放 CPU？当然了，我们现在处于未初始化状态，不需要进行调度
        proc->parent = NULL;                                  // 父进程控制块指针
        proc->mm     = NULL;                                  // 进程的内存管理字段
        memset(&(proc->context), 0, sizeof(struct context));  // 上下文，现在是源头，当然为空，发生切换时修改
        proc->tf = NULL;       // 进程中断帧，初始化为空，发生中断时修改
        proc->cr3 = boot_cr3;  // 页表基址初始化——在pmm_init中初始化页表基址，实际上是satp寄存器
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);
             //LAB5 YOUR CODE : (update LAB4 steps)
/*
 * 以下字段（在 LAB5 中添加）在 proc_struct 中需要初始化：
 *       uint32_t wait_state;                        // 等待状态
 *       struct proc_struct *cptr, *yptr, *optr;     // 进程之间的关系
 */

        proc->wait_state = 0;  // 初始化进程等待状态
        proc->cptr = NULL;     // 设置三个进程指针
        proc->optr = NULL;
        proc->yptr = NULL;    
        //cptr是子进程链表，yptr是兄弟进程链表，optr是父进程链表
    }
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - 设置进程的关系链接
static void
set_links(struct proc_struct *proc) {
    list_add(&proc_list, &(proc->list_link));  // 将进程添加到进程列表中
    proc->yptr = NULL;  // 设置进程的 yp 指针为 NULL
    if ((proc->optr = proc->parent->cptr) != NULL) {  // 如果父进程的 cptr 指针不为空
        proc->optr->yptr = proc;  // 将父进程的 optr 指针的 yp 指向当前进程
    }
    proc->parent->cptr = proc;  // 设置父进程的 cptr 指向当前进程
    nr_process++;  // 增加进程计数
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) {
        proc->yptr->optr = proc->optr;
    }
    else {
       proc->parent->cptr = proc->optr;
    }
    nr_process --;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        // LAB4:EXERCISE3 YOUR CODE
/*
 * 一些有用的宏、函数和定义，你可以在下面的实现中使用它们。
 * 宏或函数：
 *   local_intr_save():        禁用中断
 *   local_intr_restore():     启用中断
 *   lcr3():                   修改CR3寄存器的值
 *   switch_to():              在两个进程之间进行上下文切换
 */

        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            lcr3(next->cr3);
            switch_to(&(prev->context), &(next->context));
        }
        // 允许中断
        local_intr_restore(intr_flag);

    }
}

// forkret -- 新线程/进程的第一个内核入口点
// 注意：forkret 的地址在 copy_thread 函数中设置
//       在 switch_to 之后，当前进程将从这里开始执行
static void
forkret(void) {
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc) {
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - 使用 "fn" 函数创建一个内核线程
// 注意：temp trapframe tf 的内容将在 do_fork --> copy_thread 函数中
//       被复制到 proc->tf

int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - 分配大小为 KSTACKPAGE 的页作为进程的内核栈
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - 释放进程内核栈的内存空间
static void
put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - 分配一页作为页目录表（PDT）
static int
setup_pgdir(struct mm_struct *mm) {
    struct Page *page;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - 释放PDT的内存空间
static void
put_pgdir(struct mm_struct *mm) {
    free_page(kva2page(mm->pgdir));
}

// copy_mm - 根据clone_flags处理"proc"的内存管理（mm）复制或共享"current"进程的内存
//          - 如果clone_flags & CLONE_VM，表示“共享”；否则表示“复制”
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    if (oldmm == NULL) {
        return 0;
    }
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
        goto good_mm;
    }
    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - 在进程的内核栈顶部设置陷阱帧（trapframe）
//              - 设置进程的内核入口点和栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    // 设置进程的 trapframe 指针，指向内核栈的顶部
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    // 将当前进程的 trapframe 内容复制到新进程的 trapframe
    *(proc->tf) = *tf;

    // 将 a0 寄存器设置为 0，这样子进程知道它是刚刚被 fork 的
    proc->tf->gpr.a0 = 0;
    
    // 设置栈指针（sp），如果 esp 为 0，则设置为 proc->tf 的地址
    // 否则使用传入的 esp
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf - 4 : esp;

    // 设置新进程的上下文返回地址为 forkret（进程启动后返回的函数）
    proc->context.ra = (uintptr_t)forkret;
    // 设置新进程的上下文栈指针为新进程的 trapframe 地址
    proc->context.sp = (uintptr_t)(proc->tf);
}

/* do_fork -     父进程为新子进程创建一个副本
 * @clone_flags: 用于指导如何克隆子进程
 * @stack:       父进程的用户栈指针。如果 stack == 0，表示要创建一个内核线程。
 * @tf:          要复制到子进程 proc->tf 的陷阱帧信息
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

    //LAB5 YOUR CODE : （更新 LAB4 步骤）
    //提示：你应该修改你在 lab4 中写的代码（第1步和第5步），而不是添加更多代码。
   /* 一些函数
    *    set_links: 设置进程的关系链。另见：remove_links：清除进程的关系链
    *    -------------------
    *    更新第1步：将子进程的父进程设置为当前进程，确保当前进程的 wait_state 为 0
    *    更新第5步：将 proc_struct 插入到 hash_list 和 proc_list 中，并设置进程的关系链
    */

    if((proc = alloc_proc()) == NULL) {goto fork_out;}
    proc->parent = current;
    assert(current->wait_state == 0);
    if(setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    //    3. 调用copy_mm根据clone_flag复制或共享内存
    if(copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    //    4. 调用copy_thread设置tf和上下文在proc_struct中
    copy_thread(proc, stack, tf);
    bool intr_flag;
        //    5. 将proc_struct插入哈希链表和进程链表,update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);
    }
    local_intr_restore(intr_flag);
    wakeup_proc(proc);
    ret = proc->pid;
 
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - 由 sys_exit 调用
//   1. 调用 exit_mmap、put_pgdir 和 mm_destroy 释放进程几乎所有的内存空间
//   2. 将进程的状态设置为 PROC_ZOMBIE，然后调用 wakeup_proc(parent) 请求父进程回收该进程
//   3. 调用调度器切换到其他进程
int
do_exit(int error_code) {
    // 检查当前进程是否为idleproc或initproc，如果是，发出panic
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }
    // 获取当前进程的内存管理结构mm
    struct mm_struct *mm = current->mm;
    // 如果mm不为空，说明是用户进程
    if (mm != NULL) {
        // 切换到内核页表，确保接下来的操作在内核空间执行
        lcr3(boot_cr3);
        // 如果mm引用计数减到0，说明没有其他进程共享此mm
        if (mm_count_dec(mm) == 0) {
        // 释放用户虚拟内存空间相关的资源
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        // 将当前进程的mm设置为NULL，表示资源已经释放
        current->mm = NULL;
    }
    // 设置进程状态为PROC_ZOMBIE，表示进程已退出
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    bool intr_flag;
    struct proc_struct *proc;
    //关中断
    local_intr_save(intr_flag);
    {
        // 获取当前进程的父进程
        proc = current->parent;
        // 如果父进程处于等待子进程状态，则唤醒父进程
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }
        // 遍历当前进程的所有子进程，将其父进程设置为initproc
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;
            //加入initproc的子进程链表
            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            // 如果子进程处于等待子进程状态，则唤醒子进程
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

/* load_icode - 将二进制程序（ELF 格式）的内容加载为当前进程的新内容
 * @binary:  二进制程序内容的内存地址
 * @size:    二进制程序内容的大小
 */
static int
load_icode(unsigned char *binary, size_t size) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    //(1) create a new mm for current process
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    //(2) 创建一个新的页目录表（PDT），并将 mm->pgdir 设置为 PDT 的内核虚拟地址
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    //(3) 复制 TEXT/DATA 区段，将二进制中的 BSS 部分构建到进程的内存空间中
    struct Page *page;
    //(3.1) 获取二进制程序（ELF 格式）的文件头
    struct elfhdr *elf = (struct elfhdr *)binary;
    //(3.2) 获取二进制程序（ELF 格式）程序段头表的入口
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    //(3.3) 该程序有效吗？如果不是，返回错误
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph ++) {
    //(3.4) 查找每个程序段头表，如果是 ELF_PT_LOAD 类型，则加载到内存中
        if (ph->p_type != ELF_PT_LOAD) {
            continue ;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            // continue ;
        }
    //(3.5) call mm_map fun to setup the new vma ( ph->p_va, ph->p_memsz)
    //中文：调用 mm_map 函数设置新的 vma（ph->p_va，ph->p_memsz）
        vm_flags = 0, perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        // modify the perm bits here for RISC-V
        // 在这里修改 RISC-V 的 perm 位
        if (vm_flags & VM_READ) perm |= PTE_R;
        if (vm_flags & VM_WRITE) perm |= (PTE_W | PTE_R);
        if (vm_flags & VM_EXEC) perm |= PTE_X;
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

     //(3.6) alloc memory, and  copy the contents of every program section (from, from+end) to process's memory (la, la+end)
     //中文：分配内存，并将每个程序段的内容（从，从+结束）复制到进程的内存（la，la+结束）
        end = ph->p_va + ph->p_filesz;
     //(3.6.1) copy TEXT/DATA section of bianry program
     //中文：复制二进制程序的 TEXT/DATA 部分
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memcpy(page2kva(page) + off, from, size);
            start += size, from += size;
        }

      //(3.6.2) build BSS section of binary program
      //中文：构建二进制程序的 BSS 部分
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end) {
                continue ;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    //(4) build user stack memory
    //中文：构建用户栈内存
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-2*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-3*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-4*PGSIZE , PTE_USER) != NULL);
    
    //(5) set current process's mm, sr3, and set CR3 reg = physical addr of Page Directory
    //中文：设置当前进程的 mm、sr3，并设置 CR3 寄存器 = 页目录表的物理地址
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    //(6) setup trapframe for user environment
    //中文：为用户环境设置陷阱帧
    struct trapframe *tf = current->tf;
    // Keep sstatus
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5:EXERCISE1 YOUR CODE
     * should set tf->gpr.sp, tf->epc, tf->status
     * NOTICE: If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
     *          tf->gpr.sp should be user stack top (the value of sp)
     *          tf->epc should be entry point of user program (the value of sepc)
     *          tf->status should be appropriate for user program (the value of sstatus)
     *          hint: check meaning of SPP, SPIE in SSTATUS, use them by SSTATUS_SPP, SSTATUS_SPIE(defined in risv.h)
     *  应该设置 tf->gpr.sp, tf->epc, tf->status
     * 注意：如果我们正确设置了 trapframe，那么用户级进程可以从内核返回到用户模式。因此
     *          tf->gpr.sp 应该是用户栈顶（sp 的值）
     *          tf->epc 应该是用户程序的入口点（sepc 的值）
     *          tf->status 应该适合用户程序（sstatus 的值）
     *          提示：检查 SSTATUS 中 SPP 和 SPIE 的含义，使用它们通过 SSTATUS_SPP 和 SSTATUS_SPIE（在 risv.h 中定义）
     */

    tf->gpr.sp = USTACKTOP;//设置tf->gpr.sp为用户栈顶
    tf->epc = elf->e_entry;//设置tf->epc为用户程序的入口点
    // sstatus &= ~SSTATUS_SPP;
    // sstatus &= SSTATUS_SPIE;
    // tf->status = sstatus;
    tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);//设置tf->status为适合用户程序的值

    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}

// do_execve - call exit_mmap(mm)&put_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
// do_execve - 调用 exit_mmap(mm) 和 put_pgdir(mm) 释放当前进程的内存空间 
//           - 调用 load_icode 根据二进制程序设置新的内存空间
int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    struct mm_struct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {//检查name的内存空间能否被访问
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN) {//进程名字的长度有上限 PROC_NAME_LEN
        len = PROC_NAME_LEN;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    if (mm != NULL) {
        cputs("mm != NULL");
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);//把进程当前占用的内存释放，之后重新分配内存
        }
        current->mm = NULL;
    }
    //把新的程序加载到当前进程里的工作都在load_icode()函数里完成
    int ret;
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;//返回不为0，则加载失败
    }
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - 请求调度器重新调度
int
do_yield(void) {
    current->need_resched = 1;
    return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory space of kernel stack
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are free.
// do_wait - 等待一个或任何处于 PROC_ZOMBIE 状态的子进程，并释放其内核栈的内存空间 
// - 释放该子进程的 proc 结构体。 
// 注意：只有在调用 do_wait 函数之后，子进程的所有资源才会被释放。
int
do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {// 检查code_store是否合法
            return -E_INVAL;
        }
    }
//初始化变量:定义用于存储进程信息、中断标志和是否存在子进程的变量
    struct proc_struct *proc;
    bool intr_flag, haskid;
//循环检查子进程：（检查当前进程的子进程是否存在、是否满足指定 PID，并查找是否有已经处于僵尸状态的子进程。）
repeat:
    haskid = 0;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {//如果找到了进程且其父进程是当前进程，表示找到了当前进程的一个子进程
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {//如果找到的子进程处于僵尸状态，说明该子进程已经退出。
                goto found;
            }
        }
    }
    else {// 遍历当前进程的所有子进程
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }//等待子进程退出
    if (haskid) { //如果有子进程，将当前进程状态设置为等待，等待子进程退出
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();//调用调度器，将 CPU 时间片分配给其他可运行的进程，使得其他进程有机会执行。
        if (current->flags & PF_EXITING) {//检查当前进程的标志位，看是否标记为正在退出。
            do_exit(-E_KILLED);// 如果当前进程标志为正在退出，调用 do_exit 函数以错误码 -E_KILLED 退出当前进程。
        }
        goto repeat;
    }
    return -E_BAD_PROC;//如果没有子进程存在，返回错误码 -E_BAD_PROC，表示未找到符合条件的子进程。

found:// 处理找到的子进程
    //检查进程的合法性:检查找到的子进程是否是空闲进程或初始化进程，如果是，则触发 panic
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }

     //获取退出码并处理进程退出:如果提供了存储退出码的地址，将退出码存储到指定地址。
    if (code_store != NULL) {
        *code_store = proc->exit_code;
    }
	//移除进程相关信息:关闭中断，移除子进程的哈希表项和链接
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    //释放内核栈和进程结构：
    put_kstack(proc);
    kfree(proc);
    return 0;
}

// do_kill - kill process with pid by set this process's flags with PF_EXITING
int
do_kill(int pid) {
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL) {
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED) {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
// kernel_execve - 执行 SYS_exec 系统调用，用于由 user_main 内核线程调用的用户程序执行
static int
kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int64_t ret=0, len = strlen(name);
 //   ret = do_execve(name, len, binary, size);上下文切换实际上要借助中断处理的返回来完成。直接调用do_execve()是无法完成上下文切换的
    asm volatile(
        "li a0, %1\n"
        "lw a1, %2\n"
        "lw a2, %3\n"
        "lw a3, %4\n"
        "lw a4, %5\n"
    	"li a7, 10\n"
        "ebreak\n"
        "sw a0, %0\n"
        : "=m"(ret)
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory");//这里内联汇编的格式，和用户态调用ecall的格式类似，只是ecall换成了ebreak
    cprintf("ret = %d\n", ret);
    return ret;
}

#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })

#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)

// user_main - kernel thread used to exec a user program用于执行用户程序的内核线程
//实际上，就是加载了存储在这个位置的程序exit
//并在user_main这个进程里开始执行。
//这时user_main就从内核进程变成了用户进程。
static int
user_main(void *arg) {
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(exit);
#endif
    panic("user_main execve failed.\n");
}

// init_main - 第二个内核线程，用于创建 user_main 内核线程
static int
init_main(void *arg) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0) {// 等待所有子进程退出
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - 通过自身设置第一个内核线程 idleproc "idle" 
//            - 创建第二个内核线程 init_main
void
proc_init(void) {
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process ++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在 kern_init 结束时，第一个内核线程 idleproc 将执行以下工作
void
cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}
