#ifndef __KERN_MM_BEST_FIT_PMM_H__
#define __KERN_MM_BEST_FIT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager best_fit_pmm_manager;

static void         best_fit_init(void);
static void         best_fit_init_memmap(struct Page* base, size_t n);
static struct Page* best_fit_alloc_pages(size_t n);
static void         best_fit_free_pages(struct Page* base, size_t n);
static size_t       best_fit_nr_free_pages(void);
static void         best_fit_check(void);
#endif /* ! __KERN_MM_BEST_FIT_PMM_H__ */
