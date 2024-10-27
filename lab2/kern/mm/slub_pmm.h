#ifndef __KERN_MM_SLUB_PMM_H__
#define __KERN_MM_SLUB_PMM_H__

#include <pmm.h>

extern const struct pmm_manager slub_pmm_manager;

static void         slub_init(void);
static void         slub_init_memmap(struct Page* base, size_t n);
static struct Page* slub_alloc_pages(size_t n);
static void         slub_free_pages(struct Page* base, size_t n);
static size_t       slub_nr_free_pages(void);
static void         slub_check(void);

#endif /* ! __KERN_MM_SLUB_PMM_H__ */