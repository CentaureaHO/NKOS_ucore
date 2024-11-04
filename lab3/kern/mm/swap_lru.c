#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>
#include <hash_map.h>

static list_entry_t lru_list_head;
static hashmap_t    lru_entry_map;

static int _lru_init_mm(struct mm_struct* mm)
{
    list_init(&lru_list_head);
    hashmap_init(&lru_entry_map, default_hash, default_key_cmp);
    mm->sm_priv = &lru_list_head;
    return 0;
}

static int _lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    (void)mm;
    (void)swap_in;

    page->pra_vaddr = addr;

    hashmap_entry_t* entry = hashmap_find(&lru_entry_map, &page->pra_vaddr, sizeof(page->pra_vaddr));

    if (entry != NULL)
        list_del(&page->pra_page_link);
    else if (hashmap_insert(&lru_entry_map, &page->pra_vaddr, sizeof(page->pra_vaddr), page, sizeof(struct Page)) != 0)
    {
        cprintf("Error: LRU hashmap full, cannot insert page.\n");
        return -1;
    }

    list_add(&lru_list_head, &page->pra_page_link);

    return 0;
}

static int _lru_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)
{
    (void)mm;
    (void)in_tick;

    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    list_entry_t* tail = list_prev(head);

    if (tail == head)
    {
        *ptr_page = NULL;
        return 0;
    }

    *ptr_page = le2page(tail, pra_page_link);
    list_del(tail);

    hashmap_remove(&lru_entry_map, &(*ptr_page)->pra_vaddr, sizeof((*ptr_page)->pra_vaddr));

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