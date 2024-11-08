#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>
#include <hash_table.h>

#define BUCKET_SIZE 4

static hashtable_entry_t* hash_table_buckets[BUCKET_SIZE];
static hashtable_t        page_hash_table;
static list_entry_t       lru_list_head;

static size_t hash_function(uintptr_t key) { return key % BUCKET_SIZE; }

static void print_lru_list()
{
    cprintf("Current LRU List: ");
    list_entry_t* le = list_next(&lru_list_head);
    while (le != &lru_list_head)
    {
        struct Page* page = to_struct(le, struct Page, pra_page_link);
        cprintf("0x%x ", page->pra_vaddr);
        le = list_next(le);
    }
    cprintf("\n");
}

static int _lru_init_mm(struct mm_struct* mm)
{
    list_init(&lru_list_head);
    hashtable_init(&page_hash_table, BUCKET_SIZE, hash_table_buckets);
    mm->sm_priv = &lru_list_head;
    return 0;
}

static int _lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    (void)mm;
    (void)swap_in;
    page->pra_vaddr      = addr;
    page->hash_entry.key = addr;

    hashtable_entry_t* found_entry = hashtable_get(&page_hash_table, addr, hash_function);
    if (found_entry != NULL)
    {
        struct Page* found_page = to_struct(found_entry, struct Page, hash_entry);
        list_del(&found_page->pra_page_link);
        list_add(&lru_list_head, &found_page->pra_page_link);
        return 0;
    }

    hashtable_insert(&page_hash_table, &page->hash_entry, hash_function);

    list_add(&lru_list_head, &page->pra_page_link);
    cprintf("Inserted page with vaddr 0x%x into LRU list.\n", addr);
    print_lru_list();

    return 0;
}

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

static int _lru_check_swap(void)
{
    cprintf("\n\nStart lru_check_swap\n");

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 4);

    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 4);

    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 4);

    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 4);

    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 5);

    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 5);

    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 6);

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 6);

    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    cprintf("Pagefault counts: %d\n", pgfault_num);
    assert(pgfault_num == 6);

    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 6);

    cprintf("write Virt Page a in lru_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    return 0;
}

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