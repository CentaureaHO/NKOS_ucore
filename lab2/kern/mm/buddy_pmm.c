#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

static free_area_t free_area;  // 避免multiple definition问题  2024.10.02 2210878

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void         buddy_init(void) { cprintf("buddy init is not completed\n"); }
static void         buddy_init_memmap(struct Page* base, size_t n) { cprintf("buddy init memmap is not completed\n"); }
static struct Page* buddy_alloc_pages(size_t n)
{
    cprintf("buddy alloc pages is not completed\n");
    return NULL;
}
static void   buddy_free_pages(struct Page* base, size_t n) { cprintf("buddy free pages is not completed\n"); }
static size_t buddy_nr_free_pages(void)
{
    cprintf("buddy nr free pages is not completed\n");
    return 0;
}

static void basic_check(void)
{
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free                    = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page* p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free   = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

static void buddy_check(void) { cprintf("buddy check is not completed\n"); }

const struct pmm_manager buddy_pmm_manager = {
    .name          = "buddy_pmm_manager",
    .init          = buddy_init,
    .init_memmap   = buddy_init_memmap,
    .alloc_pages   = buddy_alloc_pages,
    .free_pages    = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check         = buddy_check,
};