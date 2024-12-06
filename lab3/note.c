换入：
mmu检测非法PTE_V->trap.c / trap(struct trapframe* tf)->trap.c / exception_handler(struct trapframe* tf)->trap.c /
    pgfault_handler(struct trapframe* tf)->vmm.c /
    do_pgfault(struct mm_struct* mm, uint_t error_code, uintptr_t addr)->vmm.c /
    find_vma(struct mm_struct* mm, uintptr_t addr) 获取对应内存区域->defs.h /
    #ROUNDDOWN(a, n) 对齐到页，获取页基地址->pmm.c /
    get_pte(pde_t* pgdir, uintptr_t la, bool create) 获取或分配与虚拟地址相关联的页表项->pmm.c /
    pgdir_alloc_page(pde_t* pgdir, uintptr_t la, uint32_t perm) 为首次访问的虚拟地址分配物理页并建立映射->swap.c /
    swap_in(mm, addr, &page);
尝试换入一个页面->pmm.h / #alloc_page()->pmm.c / alloc_pages(size_t n) 为新页分配物理页->pmm.c /
    get_pte(pde_t* pgdir, uintptr_t la, bool create) 获取或分配与虚拟地址相关联的页表项->swapfs.c /
    swapfs_read(swap_entry_t entry, struct Page* page) 从磁盘读取数据到内存，实际使用内存拷贝进行模拟->ide.c /
    ide_read_secs(unsigned short ideno, uint32_t secno, void* dst, size_t nsecs)->string.c /
    memcpy(void* dst, const void* src, size_t n)->pmm.c /
    page_insert(pde_t* pgdir, struct Page* page, uintptr_t la, uint32_t perm) 建立虚拟地址到物理页面的映射->swap.c /
    swap_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in) 将页加入到交换算法的管理中

    换出： mmu检测非法PTE_V 其它页换入，执行到其它页的pgdir_alloc_page或swap_in（其中会调用alloc_page()
        ->alloc_pages(1)），如果分配不成功则执行swap_out->pmm.c
    / alloc_pages(size_t n) 为新页分配一个物理页，过程中可能内存不足导致分配失败，便会进入后续swap_out流程->swap.c /
    swap_out(struct mm_struct* mm, int n, int in_tick) 选择一个页面换出->具体的swap_out_victim实现

    函数
    / 宏集合： 1. trap.c / trap(struct trapframe* tf) 2. trap.c / exception_handler(struct trapframe* tf) 3. trap.c /
    pgfault_handler(struct trapframe* tf) 4. vmm.c /
    do_pgfault(struct mm_struct* mm, uint_t error_code, uintptr_t addr) 5. vmm.c /
    find_vma(struct mm_struct* mm, uintptr_t addr) 6. defs.h / #ROUNDDOWN(a, n) 7. pmm.c /
    get_pte(pde_t* pgdir, uintptr_t la, bool create) 8. pmm.c /
    pgdir_alloc_page(pde_t* pgdir, uintptr_t la, uint32_t perm) 9. swap.c / swap_in(mm, addr, &page) 10. pmm.c /
    page_insert(pde_t* pgdir, struct Page* page, uintptr_t la, uint32_t perm) 11. swap.c /
    swap_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in) 12. pmm.h /
    #alloc_page() 13. pmm.c / alloc_pages() 14. swap.c / swap_out(struct mm_struct* mm, int n, int in_tick) 15. ide.c /
    ide_read_secs(unsigned short ideno, uint32_t secno, void* dst, size_t nsecs)