#ifndef __KERN_MM_DEFAULT_PMM_H__
#define __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager;

static void         default_init(void);
static void         default_init_memmap(struct Page* base, size_t n);
static struct Page* default_alloc_pages(size_t n);
static void         default_free_pages(struct Page* base, size_t n);
static size_t       default_nr_free_pages(void);
static void         default_check(void);
#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */
