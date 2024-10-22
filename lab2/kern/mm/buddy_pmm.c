#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>
#define MAX_ORDER 15  // 定义最大阶层，表示内存块划分的最大深度

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

    size_t offset = 0;

    while (n > 0) {
        uint32_t order = 0;
        while ((1 << order) <= n && order < MAX_ORDER) {
            order += 1;
        }
        if (order > 0) {
            order -= 1;
        }

        p = base + offset;
        p->property = 1 << order;
        SetPageProperty(p);

        list_add(&(free_list(order)), &(p->page_link));
        nr_free(order) += 1;

        offset += (1 << order);
        n -= (1 << order);
    }
}

// 将较大的内存块拆分为两个较小的块
static void split_page(int order) {
    assert(order > 0 && order < MAX_ORDER);

    if (list_empty(&(free_list(order)))) {
        split_page(order + 1);
    }

    if (list_empty(&(free_list(order)))) {
        return;
    }

    list_entry_t *le = list_next(&(free_list(order)));
    struct Page *page = le2page(le, page_link);
    list_del(&(page->page_link));
    nr_free(order) -= 1;

    uint32_t size = 1 << (order - 1);
    struct Page *buddy = page + size;

    page->property = size;
    buddy->property = size;

    SetPageProperty(page);
    SetPageProperty(buddy);

    list_add(&(free_list(order - 1)), &(page->page_link));
    list_add(&(free_list(order - 1)), &(buddy->page_link));
    nr_free(order - 1) += 2;
}

// 为用户分配页面
static struct Page *buddy_system_alloc_pages(size_t n) {
    assert(n > 0);  // 确保要分配的页数大于0
    if (n > (1 << (MAX_ORDER - 1))) {  // 请求的页数超过最大块大小
        return NULL;  // 无法满足请求
    }
    struct Page *page = NULL;

    // 正确计算需要的最小阶层
    uint32_t order = 0;
    while ((1 << order) < n && order < MAX_ORDER) {
        order += 1;
    }

    if (order >= MAX_ORDER) {
        return NULL;
    }

    uint32_t current_order = order;

    // 查找可用的块，如果当前阶层没有，则向上寻找
    while (current_order < MAX_ORDER && list_empty(&(free_list(current_order)))) {
        current_order += 1;
    }

    if (current_order == MAX_ORDER) {
        return NULL;
    }

    // 逐级拆分，直到达到所需的阶层
    while (current_order > order) {
        split_page(current_order);
        current_order -= 1;
    }

    // 从空闲链表中获取块
    list_entry_t *le = list_next(&(free_list(order)));
    page = le2page(le, page_link);
    list_del(&(page->page_link));
    nr_free(order) -= 1;
    ClearPageProperty(page);

    return page;
}

// 将内存块插入到指定阶层的空闲链表中
static void add_page(uint32_t order, struct Page *base) {
    list_add(&(free_list(order)), &(base->page_link));
}

// 合并相邻的伙伴块
// 合并相邻的伙伴块
static void merge_page(uint32_t order, struct Page *base) {
    if (order >= MAX_ORDER - 1) {
        // 当达到最高阶层时，直接将页面添加回空闲链表
        add_page(order, base);
        nr_free(order) += 1;
        return;
    }

    size_t size = 1 << order;
    uintptr_t addr = page2pa(base);
    uintptr_t buddy_addr = addr ^ (size << PGSHIFT);
    struct Page *buddy = pa2page(buddy_addr);

    if (buddy->property != size || !PageProperty(buddy)) {
        // 伙伴不可合并
        add_page(order, base);
        nr_free(order) += 1;
        return;
    }

    // 从当前阶层中移除伙伴
    list_del(&(buddy->page_link));
    ClearPageProperty(buddy);
    nr_free(order) -= 1;

    // 选择地址较小的块作为新的基地址
    if (buddy < base) {
        base = buddy;
    }

    // 设置新的属性值
    base->property = size << 1;
    ClearPageProperty(base);

    // 递归合并到更高阶层
    merge_page(order + 1, base);
}


// 释放页面，将其重新加入空闲链表中
static void buddy_system_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(IS_POWER_OF_2(n));  // 页数必须是2的幂
    assert(n <= (1 << (MAX_ORDER - 1)));  // 页数不能超过最大阶层的块大小

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
    while (temp > 1) {
        temp >>= 1;
        order++;
    }

    // 尝试合并
    merge_page(order, base);
}

// 返回系统中所有空闲页面的总数量
static size_t buddy_system_nr_free_pages(void) {
    size_t num = 0;
    for (int i = 0; i < MAX_ORDER; i++) {
        num += nr_free(i) * (1 << i);  // 计算每个阶层的空闲页数
    }
    return num;  // 返回总空闲页面数
}

// 检查伙伴系统是否正常运行
static void buddy_system_check(void) {
    size_t total_free_pages = buddy_system_nr_free_pages();
    cprintf("开始伙伴系统检查：total_free_pages = %d\n", total_free_pages);

    // 分配一个页面
    struct Page *p0 = buddy_system_alloc_pages(1);
    assert(p0 != NULL);
    cprintf("分配了1个页面。\n");

    // 检查空闲页面数量是否减少了1
    size_t free_pages_after_alloc1 = buddy_system_nr_free_pages();
    assert(free_pages_after_alloc1 == total_free_pages - 1);

    // 分配另一个页面
    struct Page *p1 = buddy_system_alloc_pages(1);
    assert(p1 != NULL);
    assert(p0 != p1);  // 确保分配了不同的页面
    cprintf("又分配了1个页面。\n");

    // 检查空闲页面数量是否又减少了1
    size_t free_pages_after_alloc2 = buddy_system_nr_free_pages();
    assert(free_pages_after_alloc2 == free_pages_after_alloc1 - 1);

    // 释放第一个页面
    buddy_system_free_pages(p0, 1);
    cprintf("释放了第一个分配的页面。\n");

    // 检查空闲页面数量是否增加了1
    size_t free_pages_after_free1 = buddy_system_nr_free_pages();
    assert(free_pages_after_free1 == free_pages_after_alloc2 + 1);

    // 释放第二个页面
    buddy_system_free_pages(p1, 1);
    cprintf("释放了第二个分配的页面。\n");

    // 检查空闲页面数量是否又增加了1，应当恢复到初始值
    size_t free_pages_after_free2 = buddy_system_nr_free_pages();
    assert(free_pages_after_free2 == total_free_pages);

    cprintf("单个页面的分配和释放测试通过。\n");

    // 现在尝试分配一个页面块
    size_t n = 4; // 尝试分配4个页面
    struct Page *p2 = buddy_system_alloc_pages(n);
    assert(p2 != NULL);
    cprintf("分配了%d页面。\n", n);

    // 检查空闲页面数量是否减少了n
    size_t free_pages_after_alloc_n = buddy_system_nr_free_pages();
    assert(free_pages_after_alloc_n == free_pages_after_free2 - n);

    // 释放页面块
    buddy_system_free_pages(p2, n);
    cprintf("释放了%d个页面。\n", n);

    // 检查空闲页面数量是否增加了n，应当恢复到初始值
    size_t free_pages_after_free_n = buddy_system_nr_free_pages();
    assert(free_pages_after_free_n == total_free_pages);

    cprintf("分配和释放%d个页面的测试通过。\n", n);

    // 尝试分配最大可能的块
    n = 1 << (MAX_ORDER - 1); // 最大块大小
    struct Page *p3 = buddy_system_alloc_pages(n);
    assert(p3 != NULL);
    cprintf("分配了最大块大小%d个页面。\n", n);

    // 检查空闲页面数量是否减少了n
    size_t free_pages_after_alloc_max = buddy_system_nr_free_pages();
    assert(free_pages_after_alloc_max == total_free_pages - n);

    // 尝试再分配一个页面，应当成功，因为还有剩余的空闲页面
    struct Page *p4 = buddy_system_alloc_pages(n);
    assert(p4 == NULL);
    cprintf("无法生成两个相同的最大块\n,因此，通过了最大页的测试。\n");


    // 释放最大块
    buddy_system_free_pages(p3, n);
    cprintf("释放了大小%d个页面。\n", n);

    // 释放额外的页面
    //buddy_system_free_pages(p4, n);
    //cprintf("释放了额外的页面。\n");

    // 检查空闲页面数量是否恢复到初始值
    size_t free_pages_after_free_max = buddy_system_nr_free_pages();
    assert(free_pages_after_free_max == total_free_pages);
    cprintf("最大块的分配和释放测试通过。\n");

    cprintf("所有伙伴系统检查均通过。\n");
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