#ifndef __KERN_MM_SLUB_PMM_H__
#define __KERN_MM_SLUB_PMM_H__

#include <pmm.h>

#define NUM_CACHES 11
#define SLUB_BASE_SHIFT 3

struct slab
{
    struct kmem_cache* cache;
    unsigned int       inuse;
    unsigned int       total_objects;
    list_entry_t       list;
};

struct kmem_cache
{
    size_t       object_size;
    size_t       size;
    void*        freelist;
    list_entry_t partial_list;
};

extern const struct pmm_manager slub_pmm_manager;

static void         slub_init(void);
static void         slub_init_memmap(struct Page* base, size_t n);
static struct Page* slub_alloc_pages(size_t n);
static void         slub_free_pages(struct Page* base, size_t n);
static size_t       slub_nr_free_pages(void);
static void         slub_check(void);

#endif /* ! __KERN_MM_SLUB_PMM_H__ */