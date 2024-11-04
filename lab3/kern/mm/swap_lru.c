#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>
#include <hash_map.h>
#include <sbi.h>

typedef struct
{
    int          capacity;   // LRU 缓存的容量
    list_entry_t page_list;  // LRU 页面链表，用于存储页面访问顺序
    hashmap_t    page_map;   // 哈希表，用于快速查找页面
} LRUCache;

void LRUCache_init(LRUCache* cache, int capacity)
{
    cache->capacity = capacity;
    list_init(&cache->page_list);                                             // 初始化页面链表
    hashmap_init(&cache->page_map, capacity, default_hash, default_key_cmp);  // 初始化哈希表
}

void accessPage(LRUCache* cache, struct mm_struct* mm, struct Page* page)
{
    uintptr_t        page_number = page->pra_vaddr;
    hashmap_entry_t* entry       = hashmap_find(&cache->page_map, &page_number, sizeof(page_number));

    if (entry != NULL)
    {
        // 页面命中，将页面移到链表前端
        list_del(&page->pra_page_link);
        list_add(&cache->page_list, &page->pra_page_link);
        mm->mmap_cache = le2vma(&page->pra_page_link, list_link);  // 更新 mmap_cache
        cprintf("Page %p accessed (HIT).\n", (void*)page_number);
    }
    else
    {
        // 页面未命中，需要插入页面
        if (list_next(&cache->page_list) != &cache->page_list && cache->capacity == 0)
        {
            // 缓存已满，移除最久未使用的页面
            list_entry_t* tail     = list_prev(&cache->page_list);
            struct Page*  lru_page = le2page(tail, pra_page_link);
            list_del(tail);
            hashmap_remove(&cache->page_map, &lru_page->pra_vaddr, sizeof(uintptr_t));
            cprintf("Page %p evicted.\n", (void*)lru_page->pra_vaddr);
            cache->capacity++;  // 腾出空间
        }
        // 插入新页面到链表头部
        page->pra_vaddr = page_number;
        list_add(&cache->page_list, &page->pra_page_link);
        hashmap_insert(&cache->page_map, &page->pra_vaddr, sizeof(uintptr_t), page, sizeof(struct Page));
        cprintf("Page %p inserted (MISS).\n", (void*)page_number);
        cache->capacity--;
    }
}

void displayCache(LRUCache* cache)
{
    cprintf("Current cache state: ");
    list_entry_t* entry;
    list_for_each(entry, &cache->page_list)
    {
        struct Page* page = le2page(entry, pra_page_link);
        cprintf("%p ", (void*)page->pra_vaddr);
    }
    cprintf("\n\n");
}

static int _lru_init_mm(struct mm_struct* mm)
{
    cprintf("\n\nTest LRU Cache\n");

    LRUCache cache;
    LRUCache_init(&cache, 3);
    mm->sm_priv = &cache.page_list;

    struct Page page1, page2, page3, page4, page5, page6, page7, page8;
    page1.pra_vaddr = 0x1000;
    page2.pra_vaddr = 0x2000;
    page3.pra_vaddr = 0x3000;
    page4.pra_vaddr = 0x4000;
    page5.pra_vaddr = 0x5000;
    page6.pra_vaddr = 0x6000;
    page7.pra_vaddr = 0x7000;
    page8.pra_vaddr = 0x8000;

    accessPage(&cache, mm, &page1);  // MISS
    displayCache(&cache);

    accessPage(&cache, mm, &page2);  // MISS
    displayCache(&cache);

    accessPage(&cache, mm, &page3);  // MISS
    displayCache(&cache);

    accessPage(&cache, mm, &page1);  // HIT
    displayCache(&cache);

    accessPage(&cache, mm, &page4);  // MISS, evict page2
    displayCache(&cache);

    accessPage(&cache, mm, &page2);  // MISS, evict page3
    displayCache(&cache);

    accessPage(&cache, mm, &page5);  // MISS, evict page1
    displayCache(&cache);

    accessPage(&cache, mm, &page6);  // MISS, evict page4
    displayCache(&cache);

    accessPage(&cache, mm, &page7);  // MISS, evict page2
    displayCache(&cache);

    accessPage(&cache, mm, &page8);  // MISS, evict page5
    displayCache(&cache);

    accessPage(&cache, mm, &page6);  // HIT
    displayCache(&cache);

    accessPage(&cache, mm, &page7);  // HIT
    displayCache(&cache);

    accessPage(&cache, mm, &page8);  // HIT
    displayCache(&cache);

    cprintf("Test completed, shutdown now\n");
    sbi_shutdown();

    return 0;
}

static int _lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    (void)mm;
    (void)addr;
    (void)page;
    (void)swap_in;
    return 0;
}

static int _lru_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)
{
    (void)mm;
    (void)ptr_page;
    (void)in_tick;
    return 0;
}

static int _lru_check_swap(void) { return 0; }

static int _lru_init(void) { return 0; }

static int _lru_set_unswappable(struct mm_struct* mm, uintptr_t addr)
{
    (void)mm;
    (void)addr;
    return 0;
}

static int _lru_tick_event(struct mm_struct* mm)
{
    (void)mm;
    return 0;
}

struct swap_manager swap_manager_lru = {
    .name            = "lru swap manager",
    .init            = &_lru_init,
    .init_mm         = &_lru_init_mm,
    .tick_event      = &_lru_tick_event,
    .map_swappable   = &_lru_map_swappable,
    .set_unswappable = &_lru_set_unswappable,
    .swap_out_victim = &_lru_swap_out_victim,
    .check_swap      = &_lru_check_swap,
};