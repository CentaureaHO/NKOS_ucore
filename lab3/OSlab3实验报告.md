# Lab3 :缺页异常和页面置换

小组成员： 2210878 唐显达  2213040 王禹衡   2210983  苑译元

## 练习1：理解基于FIFO的页面替换算法（思考题）

> 描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？（为了方便同学们完成练习，所以实际上我们的项目代码和实验指导的还是略有不同，例如我们将FIFO页面置换算法头文件的大部分代码放在了`kern/mm/swap_fifo.c`文件中，这点请同学们注意）
>
> > - 至少正确指出10个不同的函数分别做了什么？如果少于10个将酌情给分。我们认为只要函数原型不同，就算两个不同的函数。要求指出对执行过程有实际影响,删去后会导致输出结果不同的函数（例如assert）而不是cprintf这样的函数。如果你选择的函数不能完整地体现”从换入到换出“的过程，比如10个函数都是页面换入的时候调用的，或者解释功能的时候只解释了这10个函数在页面换入时的功能，那么也会扣除一定的分数

答：通过阅读代码，我们首先梳理页面置换的总体流程：

1. 当我们需要访问某个虚拟地址的时候，这个虚拟地址对应一个物理页，这个页保存在物理内存中；
2. 当发生缺页异常的时候，操作系统会调用`do_pgfault`函数，将相应出现异常的地址传入到相应的缺页处理函数，此时，使用`get_pte`函数创建对应虚拟地址映射过程中的页表和页表项：
 * 如果得到的页表项为0，表示这个页从来没被使用过，此时使用`pgdir_alloc_page`分配物理页，并使用`page_insert`建立映射。
 * 与此对应的，如果得到的页表项不为0，表示这个地址对应的页在之前已经被换出过，此时需要调用`swap_in`将页面内容从硬盘中通过`swapfs_read`读入对应的页面，并使用`page_insert`将相应的虚拟地址和物理地址的映射关系写入对应的页表项目，而后使用`swap_map_swappable`调用不同策略的`map_swappable`进行维护，保证页面置换可以正常进行。

下面，我们进行对FIFO页面置换算法中使用到的相关函数进行分析和解释：

### （一）部分管理结构：

```c
struct vma_struct
{
    struct mm_struct* vm_mm;      // the set of vma using the same PDT
    uintptr_t         vm_start;   // start addr of vma
    uintptr_t         vm_end;     // end addr of vma, not include the vm_end itself
    uint_t            vm_flags;   // flags of vma
    list_entry_t      list_link;  // linear list link which sorted by start addr of vma
};
// the control struct for a set of vma using the same PDT
struct mm_struct
{
    list_entry_t       mmap_list;   // linear list link which sorted by start addr of vma
    struct vma_struct* mmap_cache;  // current accessed vma, used for speed purpose
    pde_t*             pgdir;       // the PDT of these vma
    int                map_count;   // the count of these vma
    void*              sm_priv;     // the private data for swap manager
};
```



1. `pra_list_head`：在FIFO算法中，我们使用链表，将页面按照被访问后进入内存的时间，使用链表将他们连接起来，连接到`pra_list_head`中,当物理内存满了之后，想访问的地址映射的page在磁盘中，就需要将最早被访问，最先进入内存的页面进行替换，保证访存的正常进行。

2. `vma_struct`：这个结构体描述了一段连续的虚拟内存，`vm_start`和`vm_end`之间的位置，表示这段内存的起止位置；

   * `list_link`:关联结构体链表的结构；

   * `vm_flag`:表示内存所对应的权限，在虚拟内存中，多个虚拟内存区域（VMA）可以共享相同的内存，它们就会指向相同的`mm_struct` ;

3. `mm_struct`:把与自己有映射的 vma_struct 进行保存

   * `pgdir`:指向虚拟地址空间的页目录表；

   * `map_count`:管理的VMA数量

4. 在ucore中，我们使用`swap_manager`进行页面交换的处理，因此，如果我们修改页面交换的算法的话，可以采用修改对应的算法指针，就可以修改替换算法；

### （二）处理过程：

5. 我们使用`alloc_pages`获取空闲的页面，如果发现无法从物理内存页的分配器中获得页的时候，就会调用`swap_out`函数进行页面的换出；

6. `pgfault_handler`函数：当发生页面异常的时候，会调用这个函数进行异常处理；

7. `do_pgfault`函数:在异常处理函数中，这个函数会做进一步的处理，将异常的未被映射的地址，映射到物理页上,根据 `get_pte` 得到的页表项的内容确定页面是需要创建还是需要换入。

8. `get_pte`:查找虚拟地址对应的页表项，

   > 如果`ptep` 为0，说明页表项不存在；如果对应页表不存在，将会创建一个新的页表并返回相应的 `PTE`指针。同时需要使用 `pgdir_alloc_page` 函数分配物理页面来存储数据,若查到了页表项，说明页面已经在内存中，但是可能是一个交换条目（swap entry）。在这种情况下，需要将使用 swap_in **函数**，将页面从磁盘加载到内存；使用 page_Insert **函数**，建立物理地址与虚拟地址的映射；使用 swap_map_swappable **函数**，标记页面为可交换，将该页作为最近被用到的页面，添加到序列的队尾。

### （三）页面换入：

9. `swap_in`:页面换入的主要处理函数为`swap_in`函数,主要完成了页面的换入，将页面从磁盘加载到内存，使用复制的方式完成：

```c
int swap_in(struct mm_struct* mm, uintptr_t addr, struct Page** ptr_result)
{
    struct Page* result = alloc_page();
    assert(result != NULL);
    pte_t* ptep = get_pte(mm->pgdir, addr, 0);
    int r;
    if ((r = swapfs_read((*ptep), result)) != 0) { assert(r != 0); }
    cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep) >> 8, addr);
    *ptr_result = result;
    return 0;
}
```

函数首先调用`alloc_page`，获得一个空页，在物理页不够的时候，可能使用`swap_out`换出一部分页面，获得新的页.。

10. `alloc_page`:

```c
#define alloc_page() alloc_pages(1)
struct Page* alloc_pages(size_t n)
{
    struct Page* page = NULL;
    bool         intr_flag;
    while (1)
    {
        local_intr_save(intr_flag);
        {
            page = pmm_manager->alloc_pages(n);
        }
        local_intr_restore(intr_flag);
        if (page != NULL || n > 1 || swap_init_ok == 0) break;
        extern struct mm_struct* check_mm_struct;
        swap_out(check_mm_struct, n, 0);
    }
    return page;
}
```

我们不难看出:`alloc_page()`为一个宏定义，相当于`alloc_pages(n=1)`的情况。

11. `local_intr_save(intr_flag)`和`local_intr_restore(intr_flag)`函数主要用于保存和恢复中断的状态，以确保不会被中断打断

12. `assert(result != NULL)`:使用断言保证分配页面成功，如果分配失败，则会出发断言；

13. `swapfs_read`:从磁盘中读取数据，并将数据存储在`result`中，函数的参数包括要读

    取的磁盘交换区的位置，即 `(*ptep)` 中存储的值

### （四）页面换出

`swap_out`函数已经在前面的`alloc_pages`函数中使用，它主要负责页面换出功能的实现，在物理内存不足时，进行页面换出的操作：

```c
int swap_out(struct mm_struct* mm, int n, int in_tick)
{
    int i;
    for (i = 0; i != n; ++i)
    {
        uintptr_t v;
        // struct Page **ptr_page=NULL;
        struct Page* page;
        int r = sm->swap_out_victim(mm, &page, in_tick);
        if (r != 0)
        {
            cprintf("i %d, swap_out: call swap_out_victim failed\n", i);
            break;
        }
        v           = page->pra_vaddr;
        pte_t* ptep = get_pte(mm->pgdir, v, 0);
        assert((*ptep & PTE_V) != 0);
        if (swapfs_write((page->pra_vaddr / PGSIZE + 1) << 8, page) != 0)
        {
            cprintf("SWAP: failed to save\n");
            sm->map_swappable(mm, v, page, 0);
            continue;
        }
        else
        {
            cprintf(
                "swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v, page->pra_vaddr / PGSIZE + 1);
            *ptep = (page->pra_vaddr / PGSIZE + 1) << 8;
            free_page(page);
        }
        tlb_invalidate(mm->pgdir, v);
    }
    return i;
}
```

14. `swap_out_victim`:从代码中不难看出，函数的核心在于调用`sm->swap_out_victim`函数，而后获取将要置换的页面，存储在page中，而后获得r对应的页表项指针`ptep`,而后使用` assert((*ptep & PTE_V) != 0)`判断页面的合法性；

15. `swapfs_write`函数：通过传入磁盘交换区的偏移位置，计算出磁盘交换区的位置，将之前选出的页面写入磁盘之中；

16. `swapfs_read`：用于从磁盘读入数据。

17. `_fifo_swap_out_victim`的时候，我们使用之前维护的`pra_list_head`，将链表的第一个页面（即最早进入内存的页面）作为换出页面，使用list_prec函数获取链表头节点信息，使用`page2pa`将page转换成物理地址。

除了在上面分析中提到的相关功能函数之外，我们在进行FIFO算法的过程中，还使用了下面这些功能：

18. `page_insert`：将虚拟地址和新分配的页面的物理地址在页表内建立一个映射。

19. `swap_map_swappable`：在使用 FIFO 页面替换算法时会直接调用 `_fifo_map_swappable`，这会将新加入的页面存入 FIFO 算法所需要维护的队列（使用链表实现）的开头从而保证先进先出的实现。

20. `free_page`：将被换出的页面对应的物理页释放从而重新尝试分配。

21. `tlb_invalidate`：在页面换出之后刷新 TLB，防止地址映射和访问发生错误。



## 练习2：深入理解不同分页模式的工作原理（思考题）

get_pte()函数（位于`kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。

- get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
- 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

```c
pte_t* get_pte(pde_t* pgdir, uintptr_t la, bool create)
{
    pde_t* pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page* page;
        if (!create || (page = alloc_page()) == NULL) { return NULL; }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    pde_t* pdep0 = &((pde_t*)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    //    pde_t *pdep0 = &((pde_t *)(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page* page;
        if (!create || (page = alloc_page()) == NULL) { return NULL; }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        //   	memset(pa, 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t*)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```

### (1) get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。

答： get_pte()函数接受的参数有页目录表（`pgdir`）、线性地址（`la`）和一个bool类型的`create`变量，用来判断是否需要分配页面。函数通过查询给定的线性地址`la`对应的页表项，如果该页表项不存在，则根据`create`参数来决定是否创建页表，具体步骤如下所示：

1. 通过`PDX1(la)`获取一级页表项的索引，然后在页目录表`pgdir`中查找对应的一级页表项`pdep1`;
2. 如果一级页表项不存在(`!(*pdep1&PTE_V)`),则说明需要创建一级页表，在这种情况下，分配一个物理页面`page`并将其标记为已被引用(`set_page_ref(page,1)`);
3. 而后获取该物理页面的起始物理地址`pa`,然后使用`memset`将该物理页内容清零，以确保新分配的页表是干净的；
4. 接着，创建一个新的页表项并设置它的属性（ `PTE_U` 和 `PTE_V` ），将该页表项赋值给一级页表项`*pdep1`，以创建一级页表。
5. 然后，通过`PDX0(la)` 获取二级页表项的索引，然后在一级页表中查找对应的二级页表项 `pdep0`;
6. 如果二级页表项不存在（`!(*pdep0 & PTE_V)`），则需要创建二级页表，步骤与创建一级页表相似。
7. 最后，通过 PTE_ADDR(pte) 宏计算出虚拟地址 la 对应的页表项的物理地址，并将其返回。

> **sv32,sv39,sv48的异同点：**
>
> 1. sv32采用了二级页表的结构，sv39采用了三级页表，sv48分别使用了四级页表；
> 2. sv32的页表项大小为4字节，sv39为8字节，sv48为也为8字节；

`get_pte`函数的目的是查找或者创建特定线性地址对应的页表项，由于ucore使用sv39的分特机制，共有三级页表。

在`get_pte`中的两端代码，主要是对应于第一级和第二级页表（PDX1，PDX2）的查找/创建的过程，具体分别是创建一级页表项和二级页表项。

整个 `get_pte` 会对 Sv39 中的高两级页目录进行查找/创建以及初始化的操作，并且返回对应的最低一级页表项的内容。两段相似的代码分别对应了对不同级别 `VPN` 的操作，这种相似性的原因是因为这段代码是处理分页机制的基本操作，而 sv32、sv39 和 sv48 也采用了相似的多级页表结构。

### （2）目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

答：

​	这种写法可以满足在绝大多数情况下的页表查找和分配算法，我们只有在无法正常获取页表即页表非法的时候才会创建页表，将查找和分配合并在一个函数中，可以提升代码的分用复用效果，减少了代码的重复和开销。

## 练习3：给未被映射的地址映射上物理页（需要编程）

补充完成`do_pgfault`（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。

答：

### (一)`do_pgfault`函数的的分析与完成

​	在本部分的练习中，我们首先需要分析`do_pgfault`函数的作用：给未被映射的地址映射上物理页（也就是，当程序访问没有物理页映射的虚拟地址的时候），CPU应当进行异常的处理，抛出`page Fault`异常，接着在异常处理中调用`do_pgfault`函数进行页面的置换。

#### **（1）**函数参数：

```c
int do_pgfault(struct mm_struct* mm, uint_t error_code, uintptr_t addr)
{
    int ret = -E_INVAL;
    // 尝试找到包含 addr 的 vma
    struct vma_struct* vma = find_vma(mm, addr);
    pgfault_num++;
```

* 观察`do_pgfault`函数，我们可以看出，整个函数接收三个参数，分别为内存管理结构`mm`,错误类型代码`error_code`和引发错误的线性地址`addr`；

* 在进入函数内部之后，首先尝试查找传入的错误地址`addr`对应的虚拟内存区域`vma`，并将统计页面出错数量的pgfault_num数量加一，代表错误的数量增加。

#### **（2）**有效范围判断：

```c
    // 判断 addr 是否在 mm 的 vma 范围内？
    if (vma == NULL || vma->vm_start > addr){
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
```

* 而后判断出现错误的地址是否超出了vam的有效范围，即地址是否有效，如果无效，就跳转到对应的处理失败处理函数；

#### **（3）**权限修改：

```c
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) { perm |= (PTE_R | PTE_W); }
    addr = ROUNDDOWN(addr, PGSIZE);
    ret = -E_NO_MEM;
    pte_t* ptep = NULL;
  	ptep = get_pte(mm->pgdir, addr, 1); 
```

* 这部分代码，主要用来修改权限，根据`vma`的属性`VM_WRITE`，确定是否允许写入，并设置对应的权限标志。
* 而后，为了方便进行页面的置换，我们将目前的地址大小调整未页面大小的倍数，获取对应页表项的指针`ptep`

* `get_pte`函数的功能已经在练习一中详细地进行了介绍，在此处，如果PTE并不存在，则为页表分配物理内存，若PTE存在则判断是否启用交换机制`swap_init_ok`，若未启用则进入else分支，否则进入交换操作的处理程序。

#### **（4）下面进行我们需要完成的实验代码，完成对交换操作的执行。**

```c
if (*ptep == 0)
    {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
        {
            cprintf("do_pgfault 中的 pgdir_alloc_page 失败\n");
            goto failed;
        }
    }
```

* 如果获得的`*ptep`为0，那么代表着当前的页面没有有效的物理页面，也会跳转到failed对应的处理函数

之后，就进入了对应的分配算法。

在执行交换的操作的时候，我们需要：

1. 首先分配一个内存页并从磁盘上的交换文件加载数据到该内存页，因此我们使用` swap_in(mm, addr, &page)`将对应磁盘页的内容加载到内存中管理的页面。
2. 而后建立这个内存页`page`的物理地址和线性地址`addr`之间的映射，使用如下所示的`page_insert`函数。

```c
int page_insert(pde_t* pgdir, struct Page* page, uintptr_t la, uint32_t perm)
{
    pte_t* ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL) { return -E_NO_MEM; }
    page_ref_inc(page);
    if (*ptep & PTE_V)
    {
        struct Page* p = pte2page(*ptep);
        if (p == page) { page_ref_dec(page); }
        else { page_remove_pte(pgdir, la, ptep); }
    }
    *ptep = pte_create(page2ppn(page), PTE_V | perm);
    tlb_invalidate(pgdir, la);
    return 0;
}
```

3. 将页面标记为可交换的，我们使用`swap_map_swappable`函数，将页面标记为可交换的

4. 最后追踪页面映射的线性地址：`page->pra_vadde=addr`

   因此，最终，我们可以完成函数的实现：

```c
  else
    {if (swap_init_ok)
        {	struct Page* page = NULL;
            //(1) 根据 mm 和 addr，尝试将对应磁盘页的内容加载到内存中管理的页面。
            swap_in(mm, addr, &page);
            // (2) 根据 mm、addr 和 page，设置物理地址和逻辑地址的映射关系。
            page_insert(mm->pgdir, page, addr, perm);
            // (3) 设置该页为可交换的。
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        else
        {cprintf("没有初始化交换，但 ptep 是 %x，失败\n", *ptep);
         goto failed;
        }}
    ret = 0;
failed:
    return ret;
}
```

### （二）请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

答：从整体上看，sv39的页目录项和页表项结构类似，其中的合法位置可以用来判断页面是否存在。同时也存在一些标记位用来代表可读或者可写的情况。如下，列举一些在我们`do_pgfault`函数中对应的使用过相关标志位：

> PTE_A:表示内存页是否被访问过；PTE_D表示内存页是否被修改过；可以借助PTE_A标志位实现Clock页替换算法。

### （三）如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

#### 	**问题一：**

​	答：`trap--> trap_dispatch-->pgfault_handler-->do_pgfault`	

​	当产生页访问异常的时候，CPU会引入页访问异常的线性地址装到寄存器`CR2`中，并设置对应的错误代码errorCode说明页访问异常的类型，然后触发`Page Fault`异常。

#### 	**问题二：数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？**

​	答：

​	Page全局变量的每一项和页表中的页目录项和页表项之间有对应关系，Page是最低级的页表，目录项是一级页表，存储的内容是页表项的起始地址（二级页表），而页表项是二级页表，存储的是每个页表的开始地址，这些内容之间的关系是通过线性地址高低位不用功能的寻址体现的。

​	如果页表项映射到了物理地址，那么这个地址对应的就是`Page`中的一项。`Page` 结构体数组的每一项代表一个物理页面，并且可以通过页表项间接关联。页表项存储物理地址信息，这可以用来索引到对应的 `Page` 结构体，从而允许操作系统管理和跟踪物理内存的使用。

## 练习4：补充完成Clock页替换算法（需要编程）

首先，在完成时钟算法之前，我们需要明确什么是Clock页替换算法：

#### （一）Clock页替换算法的原理

Clock页替换的算法需要使用一下所示的方法进行替换：

1. 维护一个循环链表，用来将所有的页连接起来；

2. 同时标记一个访问位（vistied）,如果最近访问过对应的页，那么会将这个页的visited置为1；

3. 同时维护一个指针，当需要进行页面替换的时候，这个指针会在链表上进行循环遍历。会分为两种情况：

   * 指针指到的块的visited位为0，则会将这个位置的块作为替换的块。

   * 指针指导的块的visited位为1，那么会将这个保留，将其的visited位置为0，并访问下一个块。

#### （二）代码实现

需要我们实现的代码在`kern/mm/swap_clock.c`文件中，需要我们实现`_clock_init_mm`和`_clock_map_swappable`以及`_clock_swap_out_victim`三个函数的功能。下面我们就分别对这三个函数的实现过程进行介绍：

##### 初始化函数：

```c
static list_entry_t pra_list_head, *curr_ptr;
static int _clock_init_mm(struct mm_struct* mm)
{
    list_init(&pra_list_head);
    curr_ptr    = &pra_list_head;
    mm->sm_priv = &pra_list_head;
    return 0;
}
```

* `pra_list_head`表示当前链表的头部，`*curr_ptr`表示当前的指针
* 在`_clock_init_mm`中，我们将循环链表初始化位空链表，将`curr_ptr`指针指向链表头`pra_list_head`，便于后续页面替换算法的实现。

##### 页面访问检查和链表维护函数：

```c
static int _clock_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    (void)(mm);
    (void)(addr);
    (void)(swap_in);

    list_entry_t* head  = (list_entry_t*)mm->sm_priv;
    list_entry_t* entry = &(page->pra_page_link);

    assert(entry != NULL && curr_ptr != NULL);
    // list_add_before(&pra_list_head,entry);
    list_add(head->prev, entry);
    page->visited = 1;
    return 0;
}
```

* 这段代码主要的作用是针对我们访问的页面进行检查，同时将可以满足替换条件的页面插到循环链表中的末尾，并将页面的访问位visited置为1，表示当前页面已被访问。

##### 页面替换函数：

```c
static int _clock_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)
{
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    while (1)
    {
        /*LAB3 EXERCISE 4: 2210878 2210983 2213040*/
        // 编写代码
        // 遍历页面链表pra_list_head，查找最早未被访问的页面
        // 获取当前页面对应的Page结构指针
        // 如果当前页面未被访问，则将该页面从页面链表中删除，并将该页面指针赋值给ptr_page作为换出页面
        // 如果当前页面已被访问，则将visited标志置为0，表示该页面已被重新访问
        if (curr_ptr == &pra_list_head)  // 头节点没有意义，需要跳过
        {
            curr_ptr = list_next(curr_ptr);
        }

        struct Page* ptr = le2page(curr_ptr, pra_page_link);

        if (ptr->visited) { ptr->visited = 0; }
        else
        {
            cprintf("curr_ptr %p\n", curr_ptr);
            // curr_ptr需要更新
            curr_ptr = list_next(curr_ptr);
            list_del(curr_ptr->prev);
            *ptr_page = ptr;
            break;
        }
        curr_ptr = list_next(curr_ptr);
    }
    return 0;
}
```

这段代码是我们使用时钟算法进行轮转的核心代码，我们循环遍历链表以筛选可以替换的页面，如果当前的页面未被访问（visited=0）,那么我们将这个页面换出（从循环链表中删除）跳出本轮检查，如果页面被访问过（即visited=1），则将该页面在本次检查时重置，置为0，继续遍历。

**这样就完成了基于Clock的页置换算法，下面我们针对练习中的问题进行回答：**

#### （三）问题回答：比较Clock页替换算法和FIFO算法的不同。

* Clock算法：在本次实验中，我们实现的Clock算法在每次添加新的页面的时候，都会将其添加到链表的尾部，在每次换出页面的时候，遍历并查找最近没有使用或者满足条件的页面（但是，在Clock算法中，理论上应该有一个线程，负责控制指针一直遍历链表，将visited=1的位置置为0，以及后续的筛选出对应的被替换的页的位置。）
* FIFO算法：将链表看成队列，每次添加新的页面就会将页面添加到链表头部（队列的尾部），每次换出页面的时候，不管队列头部的页面是否被经常访问，都将它置换出来。

## 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）

> 如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

##### **好处和优势：**

1. **减少页表项的数量**：一个大页的页表项覆盖了更多的物理内存，因此只需要更少的页表项来管理相同的内存，这降低了页表的大小，减少了页表管理德开销；
2. **更高得内存访问性能**：使用大页时，页表得深度降低，减少了虚拟地址到物理地址的映射查找次数，此外，由于减少了页表项的数量，访问页表的开销更小，可以加快内存访问的速度；
3. **更高的TLB命中率**：使用大页时，页表项的数量降低，单一页表项对应的虚拟空间增大，因此TLB命中率会提高。

##### 坏处和风险：

1. **Page fault开销过大：**使用一个大页进行映射意味着在发生缺页异常和需要页面置换时需要把整个大页的内容（在 Sv39 下即为 1GiB）全部交换到硬盘上，在换回时也需要将所有的内容一起写回。在物理内存大小不够、进程数量较多而必须要进行置换时，这会造成程序运行速度的降低。

2. **页表项必须连续，占用内存过大**： 使用大页可能导致内部碎片；不适用于小内存系统；可能需要更多的页表维护工作。

3. **安全风险**：采用一个大页进行映射的方式还需要注意在页表项中设置这一内存的权限，如果一个程序存在着不同的段，将整个程序的代码段、数据段全部存入可执行的一个大页中可能会造成一系列的安全问题。

## 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法

### （一）LRU算法的整体思想：

​	LRU算法为最近最少使用的页替换算法，主要用于在页进行替换的过程中。整体思想为：每当需要有替换的页的时候，我们总是选择在目前的页中最少使用的页进行替换。

### （二）算法实现：

#### 1.数据结构和主要功能：

* **LRU链表(`lru_list_head`)：**

​	LRU链表用于维护当前内存中所有页面的顺序。最前面的页面是最近访问的页面，最末尾的是最久未访问的页面。当一个页面被访问时，会将它移动到链表的前端；当发生页面交换时，会选择链表末尾的页面（即最久未访问的页面）进行交换。

* **哈希表 (`page_hash_table`)：**

​	我们在实现传统LRU算法以外，我们实现了哈希表用于存储每个页面的虚拟地址和页面信息之间的映射，这样可以在O(1)的时间内完整对于页面的查询，这样可以快速地检查一个页面是否已经存在于内存之中。

* **页表 (`mm_struct`) 和页面 (`Page`) 结构**
  `mm_struct` 表示进程的内存管理结构，`Page` 结构表示单个页面的状态。每个 `Page` 结构包含一个虚拟地址和一个链表指针，用于将页面链接到LRU链表中。

#### 2.关键函数分析：

##### （1）初始化函数：`_lru_init_mm(struct mm_struct* mm)`

​	与其他的页面替换算法的实现思路类似，首先我们需要初始化内存管理结构，而后设置LRU链表和哈希表

```C
static int _lru_init_mm(struct mm_struct* mm)
{
    list_init(&lru_list_head);
    hashtable_init(&page_hash_table, BUCKET_SIZE, hash_table_buckets);
    mm->sm_priv = &lru_list_head;
    return 0;
}
```
* 首先，初始化一个空的双向链表 `lru_list_head`。`lru_list_head` 是一个全局变量，代表LRU（Least Recently Used）算法使用的链表头部。

* 而后初始化哈希表`page_hash_table`,用于存储页面的虚拟地址和页面信息的映射,

> `BUCKET_SIZE`：指定哈希表桶的大小。通常桶的大小会设定为一个常量，用来决定哈希表分配多少内存来存储页面信息。
>
> `hash_table_buckets`：这是一个数组，存储了哈希表的各个桶指针。它应该是一个预分配的内存区域，每个桶（entry）用于存储具有相同哈希值的页面

* 将内存管理结构体 `mm` 中的 `sm_priv` 指针设置为 `lru_list_head` 的地址。

##### （2）页面访问与维护函数：`(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)`

```C
static int _lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    (void)mm;
    (void)swap_in;
    uintptr_t pagebase_addr = addr & ~(PGSIZE - 1);
    page->pra_vaddr      = pagebase_addr;
    page->hash_entry.key = pagebase_addr;
    hashtable_entry_t* found_entry = hashtable_get(&page_hash_table, pagebase_addr, hash_function);
    if (found_entry != NULL)
    {
        struct Page* found_page = to_struct(found_entry, struct Page, hash_entry);
        list_del(&found_page->pra_page_link);
        list_add(&lru_list_head, &found_page->pra_page_link);
        return 0;
    }
    hashtable_insert(&page_hash_table, &page->hash_entry, hash_function);

    list_add(&lru_list_head, &page->pra_page_link);
    cprintf("Inserted page with vaddr 0x%x into LRU list.\n", pagebase_addr);
    print_lru_list();
    return 0;
}
```

这段代码是实现LRU算法的关键函数之一，下面我们展开介绍这段代码的功能：

* `uintptr_t pagebase_addr = addr & ~(PGSIZE - 1);`：将给定的虚拟地址 `addr` 对齐到页面大小的边界上。页面大小由 `PGSIZE` 定义，这里 `~(PGSIZE - 1)` 是一个掩码，用于清除 `addr` 中低位的偏移部分，确保 `pagebase_addr` 是页面的基地址（即对齐到页面的起始位置）。
* `page->pra_vaddr = pagebase_addr;  page->hash_entry.key = pagebase_addr;`:
  * 首先，计算出对应页面的基地址，`pagebase_addr` 存储到 `page` 结构体的 `pra_vaddr` 字段中。这个字段表示该页面的虚拟地址。
  * 而后，将页面的虚拟地址 `pagebase_addr` 设置为哈希表条目的键。该条目将用于将页面与其虚拟地址之间进行映射。
* 而后在在哈希表 `page_hash_table` 中查找一个给定的虚拟地址（`pagebase_addr`）的条目。使用 `hash_function` 来计算哈希值，并根据该哈希值查找页面条目。
* 如果查到了该页对应的条目：
  * 那么就代表着找到了对应的页面，说明该页面已经存在于内存之中了，那么我们只需要通过这个哈希条目找到对应的目的地址，找到对应的`Page`结构体，而后转换为实际的结构体。
  * 之后从LRU链表中将对应的页面移到LRU链表的链表头，表示它是最近被访问过的页面

* 相对应的，如果在哈希表中没有找到对应的页面，则将这个页面插入哈希表中，并调用哈希函数，来计算对应的哈希值并插入；
  * 最后输出对应的新的页面的地址和全部LRU链表

##### （3）页面替换函数：`_lru_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)`

```C
static int _lru_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)
{
    (void)mm;
    (void)in_tick;
    list_entry_t* head = &lru_list_head;
    list_entry_t* tail = list_prev(head);
    if (tail == head)
    {
        *ptr_page = NULL;
        return 0;
    }
    struct Page* page = to_struct(tail, struct Page, pra_page_link);
    *ptr_page         = page;
    hashtable_remove(&page_hash_table, &page->hash_entry, hash_function);
    list_del(&page->pra_page_link);
    return 0;
}
```

* 这段代码的功能是实现从LRU链表中选择一个页面进行交换，核心逻辑为选出链表末尾的页面，因为链表末尾的页面是截止到目前，最久未被访问的页面：

* 首先我们创建两个指针，一个是` list_entry_t* head`用于指向LRU链表头，而后创建一个`list_entry_t* tail`用于指向链表的尾部，即我们需要替换的链表。

* `if (tail == head)`条件用于判断LRU链表是否为空，如果链表为空，那么说明没有页面可以进行交换。

* ##### ` struct Page* page = to_struct(tail, struct Page, pra_page_link)`:通过 `to_struct` 宏，将链表尾部的 `list_entry_t* tail` 转换成实际的 `Page` 结构体。`pra_page_link` 是 `Page` 结构体中的一个链表指针，表示页面在 LRU 链表中的位置。`to_struct` 宏根据链表指针 `tail` 找到对应的页面 `struct Page* page`。

* ` hashtable_remove(&page_hash_table, &page->hash_entry, hash_function);`是从哈希表中移除该页面的条目，这一页面的哈希表条目是根据页面的虚拟地址进行存储的，将其中标记为不再有效，不再需要哈希表进行索引，并从链表中将其删除。

##### （4）检查函数：

```C
#define CHECK_LIST(_pos, _list, _arr, _idx, _addr0, _addr1, _addr2, _addr3)  \
    {                                                                        \
        _idx    = 0;                                                         \
        _arr[0] = _addr0;                                                    \
        _arr[1] = _addr1;                                                    \
        _arr[2] = _addr2;                                                    \
        _arr[3] = _addr3;                                                    \
        list_for_each(_pos, _list)                                           \
        {                                                                    \
            struct Page* page = to_struct(_pos, struct Page, pra_page_link); \
            assert(page->pra_vaddr == _arr[_idx]);                           \
            _idx++;                                                          \
        }                                                                    \
     }
static int _lru_check_swap(void)
{
    list_entry_t* pos = NULL;
    size_t        idx = 0;
    uintptr_t     addr_array[4];

    cprintf("\n\nStart lru_check_swap\n");
    // initial lru list: 4000 3000 2000 1000
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x4000, 0x3000, 0x2000, 0x1000);

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    sleep(1);
    // 3000 exists, move to front
    // lru list: 3000 4000 2000 1000
    assert(pgfault_num == 4);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x3000, 0x4000, 0x2000, 0x1000);

    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    sleep(1);
    // 1000 exists, move to front
    // lru list: 1000 3000 4000 2000
    assert(pgfault_num == 4);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x1000, 0x3000, 0x4000, 0x2000);

    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    sleep(1);
    // 4000 exists, move to front
    // lru list: 4000 1000 3000 2000
    assert(pgfault_num == 4);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x4000, 0x1000, 0x3000, 0x2000);

    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    sleep(1);
    // 2000 exists, move to front
    // lru list: 2000 4000 1000 3000
    assert(pgfault_num == 4);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x2000, 0x4000, 0x1000, 0x3000);

    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    sleep(1);
    // 5000 does not exist, add to front
    // remove tail: 3000; pagefault_num: 4 -> 5
    // lru list: 5000 2000 4000 1000
    assert(pgfault_num == 5);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x5000, 0x2000, 0x4000, 0x1000);

    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    sleep(1);
    // 2000 exists, move to front
    // lru list: 2000 5000 4000 1000
    assert(pgfault_num == 5);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x2000, 0x5000, 0x4000, 0x1000);

    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    sleep(1);
    // 1000 exists, move to front
    // lru list: 1000 2000 5000 4000
    assert(pgfault_num == 5);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x1000, 0x2000, 0x5000, 0x4000);

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    sleep(1);
    // 3000 does not exist, add to front
    // remove tail: 4000; pagefault_num: 5 -> 6
    // lru list: 3000 1000 2000 5000
    assert(pgfault_num == 6);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x3000, 0x1000, 0x2000, 0x5000);

    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    sleep(1);
    // 4000 does not exist, add to front
    // remove tail: 5000; pagefault_num: 6 -> 7
    // lru list: 4000 3000 1000 2000
    assert(pgfault_num == 7);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x4000, 0x3000, 0x1000, 0x2000);

    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    sleep(1);
    // 5000 does not exist, add to front
    // remove tail: 2000; pagefault_num: 7 -> 8
    // lru list: 5000 4000 3000 1000
    assert(pgfault_num == 8);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x5000, 0x4000, 0x3000, 0x1000);

    cprintf("write Virt Page a in lru_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    *(unsigned char*)0x1000 = 0x0a;
    sleep(1);
    // 1000 exists, move to front
    // lru list: 1000 5000 4000 3000
    assert(pgfault_num == 8);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x1000, 0x5000, 0x4000, 0x3000);

    *(unsigned char*)0x1020 = 0x0a;
    sleep(1);
    assert(pgfault_num == 8);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x1000, 0x5000, 0x4000, 0x3000);

    *(unsigned char*)0x4000 = 0x0a;
    *(unsigned char*)0x3000 = 0x0a;
    *(unsigned char*)0x5000 = 0x0a;
    // 按理来说，5000最后访问，应该放到最前
    // 但由于遍历顺序，5000最先放最前，随后4000，3000
    // 因此最终得到 3000 4000 5000 1000,而非5000 3000 4000 1000
    sleep(1);
    CHECK_LIST(pos, &lru_list_head, addr_array, idx, 0x3000, 0x4000, 0x5000, 0x1000);

    *(unsigned char*)0x2000 = 0x0a;
    // 缺页后换出1000，得到2000 3000 4000 5000
    return 0;
}
```

* `CHECK_LIST` 宏用于检查 LRU 链表中的页面是否按照给定的顺序排列。它接受以下参数：
  - `_pos`：链表迭代器，用于遍历链表。
  - `_list`：链表的头部指针，表示 LRU 链表的开始。
  - `_arr`：一个包含页面虚拟地址的数组，用于验证 LRU 链表中页面的顺序。
  - `_idx`：数组的索引，用于遍历 `_arr` 中的地址。
  - `_addr0, _addr1, _addr2, _addr3`：用来检查 LRU 链表的四个虚拟地址，确保它们按顺序出现在 LRU 链表中。
* `_lru_check_swap(void)`:模拟了对多个虚拟内存页面的读写操作，并检查 LRU链表中页面的顺序是否正确。

##### （5）其他辅助函数：

```C
static int _lru_init(void) { return 0; }
static int _lru_set_unswappable(struct mm_struct* mm, uintptr_t addr)
{
    (void)mm;
    (void)addr;
    return 0;
}
static int _lru_tick_event(struct mm_struct* mm)
{
    list_entry_t* head = &lru_list_head;
    list_for_each(head, &lru_list_head)
    {
        uintptr_t vaddr = to_struct(head, struct Page, pra_page_link)->pra_vaddr;
        pte_t* ptep = get_pte(mm->pgdir, vaddr, 0);
        if ((*ptep) & PTE_A)
        {
            cprintf("Accessed page 0x%x, move to front.\n", vaddr);
            list_del(head);
            list_add(&lru_list_head, head);
        }

        *ptep &= ~PTE_A;}

    return 0;
}
```

* 这些函数是在实现我们的LRU算法过程中，所需要的一些辅助函数：
  * `_lru_tick_event(struct mm_struct* mm)`:该函数用于设置某个页面为不可交换（unswappable）。
  * `_lru_tick_event(struct mm_struct* mm)`:该函数是 LRU 页面置换算法中的一部分，它会在系统的时钟滴答（tick）事件发生时被调用，目的是根据页面的访问状态来更新 LRU 链表的顺序。
    * **遍历 LRU 链表**： `list_for_each` 用于遍历 LRU 链表中的所有页面。每个页面的虚拟地址 `vaddr` 通过 `to_struct` 宏从链表节点中提取出来，获取该页面的虚拟地址。
    * **检查访问标志**： 使用 `get_pte` 函数获取虚拟地址 `vaddr` 对应的页表项（`pte_t* ptep`）。该函数根据进程的页目录 `mm->pgdir` 和虚拟地址查找页表项。然后通过检查 `PTE_A` 标志位来确定页面是否被访问过。如果该标志位被设置，表示该页面在最近的时钟周期内已经被访问过。
    * **将访问过的页面移到链表前端**： 如果页面被访问过，即 `(*ptep) & PTE_A` 为真，则：
      1. 输出日志，表明该页面被访问并将被移到链表前端。
      2. 使用 `list_del` 将当前页面从链表中删除。
      3. 使用 `list_add` 将页面重新添加到链表头部，使其成为最新访问的页面（即移到链表的最前面）
    * **清除访问标志**： 最后，通过 `*ptep &= ~PTE_A;` 清除页面的 `PTE_A` 标志。这是为了确保在下一个时钟周期，该页面的访问状态会重新计算，而不会依赖于之前的访问记录。

### 总结：

本次实验主要进行了对于缺页异常和页面置换算法的实现，首先我们来总结分析虚拟内存管理的含义：

- 虚拟内存管理：**虚拟内存是程序员和CPU访问的地址**，不是所有的虚拟地址都有对应的物理地址，若有，则地址不相等 物理地址->虚拟地址：内存地址虚拟化的过程 通过设置页表项来限定软件运行时的访问空间，（有些虚拟地址不对应物理地址），确保软件运行不越界，完成**内存访问保护功能** 建立虚拟内存到物理内存的页映射关系——**按需分页**把不经常访问的数据写入磁盘上（放入虚拟空间中），用到时在读入内存中——**页的换入与换出**



* 下面，我们对在本次实验中，使用的三种页面置换算法进行比较：

| 算法             | FIFO                     | Clock                          | LRU                            |
| ---------------- | ------------------------ | ------------------------------ | ------------------------------ |
| **实现难度**     | 简单                     | 较简单                         | 较复杂                         |
| **页面选择依据** | 页面进入内存的顺序       | 页面访问位（“时钟”指针扫描）   | 页面最近的访问历史             |
| **优点**         | 实现简单，易于理解       | 比 FIFO 更高效，避免贝拉迪异常 | 基于实际访问情况，接近最优决策 |
| **缺点**         | 无法考虑页面访问频率     | 可能扫描多个页面               | 需要维护访问历史，开销较大     |
| **性能表现**     | 较差，可能出现贝拉迪异常 | 比 FIFO 更好                   | 性能最优，尤其在内存不足时     |
| **适用场景**     | 内存较小或不常用的系统   | 中等负载且对性能有要求的场景   | 高性能需求的系统，尤其是多任务 |