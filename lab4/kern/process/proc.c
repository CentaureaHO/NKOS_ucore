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

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one
threads for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore
needs to manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's
memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

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
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this
process SYS_sleep       : process sleep                           -->do_sleep SYS_kill        : kill process
-->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
//所有进程控制块的双向线性列表
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
//所有进程控制块的哈希表，proc_struct中的成员变量hash_link将基于pid链接入这个哈希表中
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct* idleproc = NULL;
// init proc
//本实验中，指向一个内核线程。本实验以后，此指针将指向第一个用户态进程
struct proc_struct* initproc = NULL;
// current proc
//当前占用CPU且处于“运行”状态进程控制块指针
struct proc_struct* current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe* tf);
void switch_to(struct context* from, struct context* to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct* alloc_proc(void)
{
    struct proc_struct* proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        // LAB4:EXERCISE1 YOUR CODE
        /*
         * below fields in proc_struct need to be initialized
         *       enum proc_state state;                      // Process state
         *       int pid;                                    // Process ID
         *       int runs;                                   // the running times of Proces
         *       uintptr_t kstack;                           // Process kernel stack
         *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
         *       struct proc_struct *parent;                 // the parent process
         *       struct mm_struct *mm;                       // Process's memory management field
         *       struct context context;                     // Switch here to run process
         *       struct trapframe *tf;                       // Trap frame for current interrupt
         *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
         *       uint32_t flags;                             // Process flag
         *       char name[PROC_NAME_LEN + 1];               // Process name
         */
        proc->state       = PROC_UNINIT;
        proc->pid         = -1;
        proc->runs        = 0;
        proc->kstack      = 0;
        proc->need_resched = 0;
        proc->parent       = NULL;
        proc->mm           = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf   = NULL;
        proc->cr3  = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);
    }
    return proc;
}

// set_proc_name - set the name of proc
char* set_proc_name(struct proc_struct* proc, const char* name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char* get_proc_name(struct proc_struct* proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct* proc;
    list_entry_t *      list      = &proc_list, *le;
    static int          next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID) { last_pid = 1; }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) { next_safe = proc->pid; }
        }
    }
    return last_pid;
}

// proc_run - 将进程"proc"切换到CPU上运行
// 注意：在调用switch_to之前，应该加载"proc"的新PDT基地址
void proc_run(struct proc_struct* proc)
{
    if (proc != current)
    {
        // LAB4:EXERCISE3 YOUR CODE
        /*
         * 一些有用的宏、函数和定义，下面的实现中可以使用它们。
         * 宏或函数：
         *   local_intr_save():        禁用中断
         *   local_intr_restore():     启用中断
         *   lcr3():                   修改CR3寄存器的值
         *   switch_to():              在两个进程之间进行上下文切换
         */
        bool intr_flag;
        struct proc_struct* from = current, *to = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            lcr3(to->cr3);
            switch_to(&(from->context), &(to->context));
        }
        // 允许中断
        local_intr_restore(intr_flag);
    }
}


// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void forkret(void) { forkrets(current->tf); }

// hash_proc - add proc into proc hash_list
static void hash_proc(struct proc_struct* proc) { list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link)); }

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct* find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct* proc = le2proc(le, hash_link);
            if (proc->pid == pid) { return proc; }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to
//       proc->tf in do_fork-->copy_thread function
//kernel_thread函数采用了局部变量tf来放置保存内核线程的临时中断帧，
//并把中断帧的指针传递给do_fork函数
int kernel_thread(int (*fn)(void*), void* arg, uint32_t clone_flags)
{
    // 对trameframe，也就是我们程序的一些上下文进行一些初始化
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    // 设置内核线程的参数和函数指针    
    tf.gpr.s0 = (uintptr_t)fn;   // s0 寄存器保存函数指针
    tf.gpr.s1 = (uintptr_t)arg;  // s1 寄存器保存函数参数
    // 设置 trapframe 中的 status 寄存器（SSTATUS）
    // SSTATUS_SPP：Supervisor Previous Privilege（设置为 supervisor 模式，因为这是一个内核线程）
    // SSTATUS_SPIE：Supervisor Previous Interrupt Enable（设置为启用中断，因为这是一个内核线程）
    // SSTATUS_SIE：Supervisor Interrupt Enable（设置为禁用中断，因为我们不希望该线程被中断）    
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    // 将入口点（epc）设置为 kernel_thread_entry 函数，作用实际上是将pc指针指向它(*trapentry.S会用到)
    tf.epc    = (uintptr_t)kernel_thread_entry;
    // 使用 do_fork 创建一个新进程（内核线程），这样才真正用设置的tf创建新进程。
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int setup_kstack(struct proc_struct* proc)
{
    struct Page* page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void put_kstack(struct proc_struct* proc) { free_pages(kva2page((void*)(proc->kstack)), KSTACKPAGE); }

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int copy_mm(uint32_t clone_flags, struct proc_struct* proc)
{
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void copy_thread(struct proc_struct* proc, uintptr_t esp, struct trapframe* tf)
{
    //首先在上面分配的内核栈上分配出一片空间来保存trapframe
    proc->tf    = (struct trapframe*)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;
    //然后，我们将trapframe中的a0寄存器（返回值）设置为0，说明这个进程是一个子进程
    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
    //之后我们将上下文中的ra设置为了forkret函数的入口，并且把trapframe放在上下文的栈顶
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

/* do_fork -     父进程为新的子进程创建
 * @clone_flags: 用于指导如何克隆子进程
 * @stack:       父进程的用户栈指针。如果stack==0，表示要克隆一个内核线程。
 * @tf:          中断帧信息，将被复制到子进程的proc->tf中
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe* tf)
{
    int                 ret = -E_NO_FREE_PROC;
    struct proc_struct* proc;
    if (nr_process >= MAX_PROCESS) { goto fork_out; }
    ret = -E_NO_MEM;
    // LAB4:EXERCISE2 YOUR CODE
    /*
     * 一些有用的宏、函数和定义，下面的实现中可以使用它们。
     * 宏或函数：
     *   alloc_proc:   创建一个proc结构并初始化字段（lab4:exercise1）
     *   setup_kstack: 为进程分配大小为KSTACKPAGE的内核栈
     *   copy_mm:      进程"proc"复制或共享进程"current"的内存管理系统(mm)，依据clone_flags
     *                 如果clone_flags & CLONE_VM，则"共享"；否则"复制"
     *   copy_thread:  在进程的内核栈顶部设置中断帧，并
     *                 设置进程的内核入口点和栈
     *   hash_proc:    将进程添加到进程哈希链表中
     *   get_pid:      为进程分配一个唯一的pid
     *   wakeup_proc:  设置proc->state = PROC_RUNNABLE，使进程就绪
     * 变量：
     *   proc_list:    进程集合的链表
     *   nr_process:   当前进程集合的数量
     * 
     * 分配并初始化进程控制块（alloc_proc函数）
     * 分配并初始化内核栈（setup_stack函数）
     * 根据clone_flags决定是复制还是共享内存管理系统（copy_mm函数）
     * 设置进程的中断帧和上下文（copy_thread函数）
     * 把设置好的进程加入链表
     * 将新建的进程设为就绪态
     * 将返回值设为线程id
     */

    //    1. 调用alloc_proc分配一个proc_struct
    proc = alloc_proc();
    proc->pid=get_pid();
    proc->parent=current;
    //    2. 调用setup_kstack为子进程分配一个内核栈
    setup_kstack(proc);
    //    3. 调用copy_mm根据clone_flag复制或共享内存
    copy_mm(clone_flags, proc);
    //    4. 调用copy_thread设置tf和上下文在proc_struct中
    copy_thread(proc, stack, tf);
    //    5. 将proc_struct插入哈希链表和进程链表
    hash_proc(proc);
    list_add_before(&proc_list, &(proc->list_link));
    nr_process++;
    //    6. 调用wakeup_proc将新进程设为可运行状态
    proc->state = PROC_RUNNABLE;
    //    7. 使用子进程的pid设置ret值
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code) { panic("process exit!!.\n"); }

// init_main - the second kernel thread used to create user_main kernel threads
static int init_main(void* arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char*)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and
//           - create the second kernel thread init_main
void proc_init(void)
{
    int i;
    //初始化进程控制块的双向链表和哈希表
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++) { list_init(hash_list + i); }
    //初始化idleproc内核线程
    if ((idleproc = alloc_proc()) == NULL) { panic("cannot alloc idleproc.\n"); }
    //设置idleproc内核线程的进程ID为0
    // check the proc structure
    int* context_mem = (int*)kmalloc(sizeof(struct context));
    memset(context_mem, 0, sizeof(struct context));
    int context_init_flag = memcmp(&(idleproc->context), context_mem, sizeof(struct context));

    int* proc_name_mem = (int*)kmalloc(PROC_NAME_LEN);
    memset(proc_name_mem, 0, PROC_NAME_LEN);
    int proc_name_flag = memcmp(&(idleproc->name), proc_name_mem, PROC_NAME_LEN);
    // proc->state = PROC_UNINIT;  设置进程为“初始”态
    //proc->pid = -1;             设置进程pid的未初始化值
    //proc->cr3 = boot_cr3;       使用内核页目录表的基址
    if (idleproc->cr3 == boot_cr3 && idleproc->tf == NULL && !context_init_flag && idleproc->state == PROC_UNINIT &&
        idleproc->pid == -1 && idleproc->runs == 0 && idleproc->kstack == 0 && idleproc->need_resched == 0 &&
        idleproc->parent == NULL && idleproc->mm == NULL && idleproc->flags == 0 && !proc_name_flag)
    {
        cprintf("alloc_proc() correct!\n");
    }
    //idleproc内核线程进行进一步初始化
    idleproc->pid          = 0;
    idleproc->state        = PROC_RUNNABLE;
    idleproc->kstack       = (uintptr_t)bootstack;
    idleproc->need_resched = 1;//只要此标志为1，马上就调用schedule函数要求调度器切换其他进程执行。
    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;

    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0) { panic("create init_main failed.\n"); }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched) { schedule(); }
    }
}
