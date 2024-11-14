#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>
#include <hash_table.h>
#include <clock.h>

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

        *ptep &= ~PTE_A;
    }

    return 0;
}

struct swap_manager swap_manager_lru = {.name = "lru swap manager",
    .init                                     = &_lru_init,
    .init_mm                                  = &_lru_init_mm,
    .tick_event                               = &_lru_tick_event,
    .map_swappable                            = &_lru_map_swappable,
    .set_unswappable                          = &_lru_set_unswappable,
    .swap_out_victim                          = &_lru_swap_out_victim,
    .check_swap                               = &_lru_check_swap};