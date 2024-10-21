#include <pmm.h>           
#include <list.h>          
#include <string.h>       
#include <buddy_pmm.h>     
#include <stdio.h>        

#define MAX_ORDER 11  // 定义最大阶层，表示内存块划分的最大深度

// 定义不同阶层的空闲内存块列表结构
static free_area_t free_area[MAX_ORDER];  // 每个阶层存储对应的空闲块链表和空闲块数量

// 宏定义，简化获取指定阶层的空闲链表和空闲块数量的操作
#define free_list(i) free_area[(i)].free_list  
#define nr_free(i) free_area[(i)].nr_free      

// 判断一个数字是否是2的幂
int IS_POWER_OF_2(int x) {
    return x > 0 && !(x & (x - 1));  // 如果x是2的幂次方返回真
}

// 初始化指定阶层的空闲内存块链表
static void init_free_area(int order) {
    list_init(&(free_area[order].free_list));  // 初始化链表为空
    free_area[order].nr_free = 0;  // 空闲块数量初始化为0
}

// 初始化整个伙伴系统，初始化所有阶层的空闲内存块链表
static void buddy_system_init(void) {
    for (int i = 0; i < MAX_ORDER; i++) {  
        init_free_area(i);  // 调用init_free_area初始化每一阶层
    }
}

// 初始化物理内存映射，将物理内存页加入到伙伴系统管理中
static void buddy_system_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);  // 确保有页可用
    struct Page *p = base;

    // 遍历每个页，清除属性和引用计数，确保页被释放
    for (; p != base + n; p++) {
        assert(PageReserved(p));  // 页必须被保留
        p->flags = p->property = 0;  // 清除页的标志和属性
        set_page_ref(p, 0);  // 设置引用计数为0
    }

    // 开始初始化内存块
    size_t curr_size = n;  // 剩余需要初始化的页数
    uint32_t order = MAX_ORDER - 1;  // 从最大阶层开始
    uint32_t order_size = 1 << order;  // 当前阶层的块大小（2^order）
    p = base;  // 从基地址开始

    // 依次按阶层分配内存块
    while (curr_size != 0) {
        p->property = order_size;  // 设置块大小
        SetPageProperty(p);  // 设置页的属性，表示其已分配
        nr_free(order) += 1;  // 增加当前阶层的空闲块数量
        list_add_before(&(free_list(order)), &(p->page_link));  // 加入空闲链表
        curr_size -= order_size;  // 更新剩余页数
        while (order > 0 && curr_size < order_size) {
            order_size >>= 1;  // 如果剩余内存不足当前阶层，减少阶层
            order -= 1;
        }
        p += order_size;  // 移动到下一个块
    }
}

// 将较大的内存块拆分为两个较小的块
static void split_page(int order) {
    if (list_empty(&(free_list(order)))) {  // 如果当前阶层没有空闲块
        split_page(order + 1);  // 递归地拆分更高阶层的块
    }
    // 获取当前阶层的空闲块并将其拆分
    list_entry_t *le = list_next(&(free_list(order)));  
    struct Page *page = le2page(le, page_link);  
    list_del(&(page->page_link));  // 从空闲链表中删除该块
    nr_free(order) -= 1;  // 更新空闲块数量

    uint32_t n = 1 << (order - 1);  // 计算拆分后的块大小
    struct Page *p = page + n;  // 找到第二个块的起始地址
    page->property = n;  // 设置第一个块的大小
    p->property = n;  // 设置第二个块的大小
    SetPageProperty(p);  // 设置属性，表示已分配
    // 将拆分后的两个块加入到较低阶层
    list_add(&(free_list(order - 1)), &(page->page_link));  
    list_add(&(page->page_link), &(p->page_link));  
    nr_free(order - 1) += 2;  // 更新较低阶层的空闲块数量
}

// 为用户分配页面
static struct Page *buddy_system_alloc_pages(size_t n) {
    assert(n > 0);  // 确保要分配的页数大于0
    if (n > (1 << (MAX_ORDER - 1))) {  // 请求的页数超过最大块大小
        return NULL;  // 无法满足请求
    }
    struct Page *page = NULL;
    uint32_t order = MAX_ORDER - 1;  // 从最大阶层开始寻找合适的块

    // 查找最小的可以满足请求的阶层
    while (n < (1 << order)) {
        order -= 1;  // 降低阶层
    }
    order += 1;  // 确定最终使用的阶层

    uint32_t flag = 0;  // 标记是否有可用块
    for (int i = order; i < MAX_ORDER; i++) {
        flag += nr_free(i);  // 统计该阶层及以上阶层的空闲块
    }
    if (flag == 0) return NULL;  // 如果没有足够的空闲块，返回空指针

    // 如果该阶层没有空闲块，则尝试拆分
    if (list_empty(&(free_list(order)))) {
        split_page(order + 1);  // 拆分更高阶层的块
    }
    // 再次检查空闲块是否可用
    if (list_empty(&(free_list(order)))) return NULL;

    // 从空闲链表中获取一个块并返回
    list_entry_t *le = list_next(&(free_list(order)));  
    page = le2page(le, page_link);  
    list_del(&(page->page_link));  // 从链表中删除该块
    ClearPageProperty(page);  // 清除页的属性
    return page;  // 返回分配的页
}

// 按地址顺序将内存块插入到指定阶层的空闲链表中
static void add_page(uint32_t order, struct Page *base) {
    if (list_empty(&(free_list(order)))) {  // 如果空闲链表为空
        list_add(&(free_list(order)), &(base->page_link));  // 直接插入
    } else {
        // 如果链表不为空，则按地址顺序插入
        list_entry_t *le = &(free_list(order));  
        while ((le = list_next(le)) != &(free_list(order))) {  
            struct Page *page = le2page(le, page_link);  
            if (base < page) {  // 找到插入点，按地址顺序插入
                list_add_before(le, &(base->page_link));//当base < page时，找到第一个大于base的页，将base插入到它前面，并退出循环
                break;
            } else if (list_next(le) == &(free_list(order))) {  // 如果到了链表末尾
                list_add(le, &(base->page_link));
            }
        }
    }
}

// 合并相邻的伙伴块
static void merge_page(uint32_t order, struct Page *base) {
    if (order == MAX_ORDER - 1) {  // 已经是最大阶层，不能再合并
        return;
    }

    // 尝试合并前一个块
    list_entry_t *le = list_prev(&(base->page_link));  
    if (le != &(free_list(order))) {  //判断是否已经遍历到空闲链表的末尾
        struct Page *p = le2page(le, page_link);  
        if (p + p->property == base) {  // 如果前一个块和当前块相邻
            p->property += base->property;  // 合并两个块
            ClearPageProperty(base);  //清除页面 base 的属性
            list_del(&(base->page_link));  
            base = p;  // 更新基地址
            if (order != MAX_ORDER - 1) {  
                list_del(&(base->page_link));  
                add_page(order + 1, base);  // 合并后的块加入更高阶层
            }
        }
    }

    // 尝试合并后一个块
    le = list_next(&(base->page_link));  
    if (le != &(free_list(order))) {  
        struct Page *p = le2page(le, page_link);  
        if (base + base->property == p) {  // 如果当前块和后一个块相邻
            base->property += p->property;  // 合并两个块
            ClearPageProperty(p);  
            list_del(&(p->page_link));  
            if (order != MAX_ORDER - 1) {  
                list_del(&(base->page_link));  
                add_page(order + 1, base);  // 合并后的块加入更高阶层
            }
        }
    }
    // 递归尝试合并更高阶层的块
    merge_page(order + 1, base);  
}

// 释放页面，将其重新加入空闲链表中
static void buddy_system_free_pages(struct Page *base, size_t n) {
    assert(n > 0);  
    assert(IS_POWER_OF_2(n));  // 页数必须是2的幂
    assert(n < (1 << (MAX_ORDER - 1)));  // 页数不能超过最大阶层的块大小

    struct Page *p = base;
    // 遍历每个页，清除标志位和引用计数
    for (; p != base + n; p++) {  
        assert(!PageReserved(p) && !PageProperty(p));  
        p->flags = 0;  
        set_page_ref(p, 0);  
    }

    base->property = n;  // 设置基础块大小
    SetPageProperty(base);  

    uint32_t order = 0;  // 计算对应的阶层
    size_t temp = n;
    while (temp != 1) {
        temp >>= 1;  
        order++;  
    }

    // 将块加入到空闲链表中并尝试合并
    add_page(order, base);  
    merge_page(order, base);  
}

// 返回系统中所有空闲页面的总数量
static size_t buddy_system_nr_free_pages(void) {
    size_t num = 0;  
    for (int i = 0; i < MAX_ORDER; i++) {  
        num += nr_free(i) << i;  // 计算每个阶层的空闲页数
    }//计算阶层 i 中所有空闲块所包含的总页面数量。
    return num;  // 返回总空闲页面数
}

// 检查伙伴系统是否正常运行
static void buddy_system_check(void) {
    cprintf("Starting buddy system check...\n");

    // 打印每个阶层的空闲块数量
    for (int i = 0; i < MAX_ORDER; i++) {
        size_t free_blocks = nr_free(i);
        size_t block_size = 1 << i;
        cprintf("Order %d: Free blocks = %016lx, Block size = %016lx pages\n", i, free_blocks, block_size);
    }

    // 测试分配和释放页面的功能
    struct Page *p0 = buddy_system_alloc_pages(1);
    struct Page *p1 = buddy_system_alloc_pages(1);
    struct Page *p2 = buddy_system_alloc_pages(1);
    
    assert(p0 != NULL);
    assert(p1 != NULL);
    assert(p2 != NULL);
    cprintf("Allocated 3 pages successfully.\n");

    size_t free_pages_after_alloc = buddy_system_nr_free_pages();
    cprintf("Free pages after allocation: %016lx\n", free_pages_after_alloc);

    buddy_system_free_pages(p0, 1);
    buddy_system_free_pages(p1, 1);
    buddy_system_free_pages(p2, 1);
    cprintf("Freed 3 pages successfully.\n");

    size_t free_pages_after_free = buddy_system_nr_free_pages();
    cprintf("Free pages after freeing: %016lx\n", free_pages_after_free);

    assert(free_pages_after_alloc == free_pages_after_free);
    cprintf("Buddy system check passed. All free pages restored correctly.\n");
}

// 定义伙伴系统内存管理结构体
const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_pmm_manager",  // 内存管理器名称
    .init = buddy_system_init,  // 初始化函数
    .init_memmap = buddy_system_init_memmap,  // 初始化内存映射函数
    .alloc_pages = buddy_system_alloc_pages,  // 分配页面函数
    .free_pages = buddy_system_free_pages,  // 释放页面函数
    .nr_free_pages = buddy_system_nr_free_pages,  // 获取空闲页面数量函数
    .check = buddy_system_check,  // 检查函数
};
