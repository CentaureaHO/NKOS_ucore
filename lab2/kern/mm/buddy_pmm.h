#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

extern const struct pmm_manager buddy_pmm_manager;

static void         buddy_init(void);
static void         buddy_init_memmap(struct Page* base, size_t n);
static struct Page* buddy_alloc_pages(size_t n);
static void         buddy_free_pages(struct Page* base, size_t n);
static size_t       buddy_nr_free_pages(void);
static void         buddy_check(void);

#endif