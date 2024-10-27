#include <pmm.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>
#include <slub_pmm.h>
#include <stdio.h>
#include <defs.h>

#define NUM_CACHES 11
#define SLUB_BASE_SHIFT 3

static size_t kmem_cache_sizes[NUM_CACHES];

struct slab
{
    list_entry_t       list;
    void*              freelist;
    unsigned int       inuse;
    unsigned int       total_objects;
    struct kmem_cache* cache;
};

struct kmem_cache
{
    size_t       object_size;
    size_t       size;
    list_entry_t slabs_full;
    list_entry_t slabs_partial;
    list_entry_t slabs_free;
};

static struct kmem_cache kmem_caches[NUM_CACHES];

static list_entry_t free_page_list;

static void slub_init(void)
{
    for (size_t i = SLUB_BASE_SHIFT; i < NUM_CACHES + SLUB_BASE_SHIFT; ++i)
        kmem_cache_sizes[i - SLUB_BASE_SHIFT] = (size_t)(1 << i);
    for (int i = 0; i < NUM_CACHES; ++i)
    {
        struct kmem_cache* cache = &kmem_caches[i];
        cache->object_size       = kmem_cache_sizes[i];
        cache->size              = cache->object_size;
        list_init(&(cache->slabs_full));
        list_init(&(cache->slabs_partial));
        list_init(&(cache->slabs_free));
    }
    list_init(&free_page_list);
}

static void slub_init_memmap(struct Page* base, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        struct Page* page = base + i;
        set_page_ref(page, 0);
        list_add_before(&free_page_list, &(page->page_link));
    }
}

static struct slab* create_new_slab(struct kmem_cache* cache)
{
    struct Page* page = alloc_page();
    if (page == NULL) return NULL;
    void* slab_mem = page2kva(page);

    struct slab* slab   = (struct slab*)slab_mem;
    slab->cache         = cache;
    slab->inuse         = 0;
    slab->total_objects = (PGSIZE - sizeof(struct slab)) / cache->size;
    char* obj           = (char*)(slab + 1);
    slab->freelist      = obj;
    int i;
    for (i = 0; i < slab->total_objects; ++i)
    {
        if (i == slab->total_objects - 1)
            *((void**)obj) = NULL;
        else
            *((void**)obj) = obj + cache->size;
        obj += cache->size;
    }
    return slab;
}

static void* slab_alloc_object(struct slab* slab)
{
    if (slab->freelist == NULL) return NULL;
    void* object   = slab->freelist;
    slab->freelist = *((void**)object);
    slab->inuse++;
    return object;
}

static void slab_free_object(struct slab* slab, void* object)
{
    *((void**)object) = slab->freelist;
    slab->freelist    = object;
    slab->inuse--;
}

static struct slab* get_slab_from_object(void* object)
{
    uintptr_t slab_addr = ROUNDDOWN((uintptr_t)object, PGSIZE);
    return (struct slab*)slab_addr;
}

static void* slub_alloc(size_t size)
{
    int                i;
    struct kmem_cache* cache = NULL;
    for (i = 0; i < NUM_CACHES; ++i)
    {
        if (size <= kmem_cache_sizes[i])
        {
            cache = &kmem_caches[i];
            break;
        }
    }
    if (cache == NULL)
    {
        size_t       npages = (size + PGSIZE - 1) / PGSIZE;
        struct Page* page   = slub_alloc_pages(npages);
        if (page == NULL) return NULL;
        return page2kva(page);
    }

    struct slab* slab   = NULL;
    void*        object = NULL;

    if (!list_empty(&(cache->slabs_partial)))
    {
        list_entry_t* le = list_next(&(cache->slabs_partial));
        slab             = le2struct(le, struct slab, list);
        object           = slab_alloc_object(slab);
    }
    else if (!list_empty(&(cache->slabs_free)))
    {
        list_entry_t* le = list_next(&(cache->slabs_free));
        slab             = le2struct(le, struct slab, list);
        object           = slab_alloc_object(slab);
    }
    else
    {
        slab = create_new_slab(cache);
        if (slab == NULL) return NULL;
        object = slab_alloc_object(slab);
    }

    list_del(&(slab->list));

    if (slab->inuse == slab->total_objects)
        list_add(&(cache->slabs_full), &(slab->list));
    else
        list_add(&(cache->slabs_partial), &(slab->list));

    return object;
}

static void slub_free(void* object, size_t size)
{
    if (object == NULL) return;

    int                i;
    struct kmem_cache* cache = NULL;
    for (i = 0; i < NUM_CACHES; ++i)
    {
        if (size <= kmem_cache_sizes[i])
        {
            cache = &kmem_caches[i];
            break;
        }
    }
    if (cache == NULL)
    {
        size_t       npages = (size + PGSIZE - 1) / PGSIZE;
        struct Page* page   = kva2page(object);
        slub_free_pages(page, npages);
        return;
    }

    struct slab* slab = get_slab_from_object(object);
    slab_free_object(slab, object);
    list_del(&(slab->list));

    if (slab->inuse == 0)
        list_add(&(cache->slabs_free), &(slab->list));
    else if (slab->inuse < slab->total_objects)
        list_add(&(cache->slabs_partial), &(slab->list));
}

static struct Page* slub_alloc_pages(size_t n)
{
    if (n == 0) return NULL;

    list_entry_t* le = list_next(&free_page_list);

    while (le != &free_page_list)
    {
        struct Page*  start_page = le2page(le, page_link);
        list_entry_t* current_le = le;
        size_t        i;

        for (i = 0; i < n; ++i)
        {
            if (current_le == &free_page_list) break;
            struct Page* page = le2page(current_le, page_link);
            if (page != start_page + i) break;
            current_le = list_next(current_le);
        }

        if (i == n)
        {
            current_le = le;
            for (i = 0; i < n; ++i)
            {
                list_entry_t* next_le = list_next(current_le);
                list_del(current_le);
                current_le = next_le;
            }
            return start_page;
        }
        else { le = list_next(le); }
    }
    return NULL;
}

static void slub_free_pages(struct Page* base, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        struct Page*  page = base + i;
        list_entry_t* le   = list_next(&free_page_list);
        while (le != &free_page_list)
        {
            struct Page* p = le2page(le, page_link);
            if (page < p) { break; }
            le = list_next(le);
        }
        list_add_before(le, &(page->page_link));
    }
}

static size_t slub_nr_free_pages(void)
{
    size_t        count = 0;
    list_entry_t* le    = list_next(&free_page_list);
    while (le != &free_page_list)
    {
        count++;
        le = list_next(le);
    }
    return count;
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

    free_page(p0);
    free_page(p1);
    free_page(p2);
}

static void slub_check(void)
{
    cprintf("slub_check() called\n");

    basic_check();

    size_t nr_free_pages_record = nr_free_pages();
    cprintf("nr_free_pages() = %d\n", nr_free_pages_record);

    struct Page *p0, *p1, *p2, *p3;
    p0 = p1 = p2 = p3 = NULL;

    assert((p0 = alloc_pages(1)) != NULL);
    assert((p1 = alloc_pages(2)) != NULL);
    assert((p2 = alloc_pages(3)) != NULL);
    cprintf("Successfully allocated 6 pages for p0: 1, p1: 2, p2: 3\n");

    assert(nr_free_pages() == nr_free_pages_record - 6);
    cprintf("nr_free_pages() = %d\n", nr_free_pages());

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    cprintf("p0 = %p, p1 = %p, p2 = %p\n", p0, p1, p2);
    cprintf("Byte difference(oct): (p1 - p0) = %d, (p2 - p1) = %d\n",
        (int)((uintptr_t)p1 - (uintptr_t)p0),
        (int)((uintptr_t)p2 - (uintptr_t)p1));

    free_pages(p1, 2);
    assert(nr_free_pages() == nr_free_pages_record - 4);
    cprintf("free p1: 2 pages, nr_free_pages() = %d\n", nr_free_pages());

    assert((p3 = alloc_pages(2)) != NULL);
    cprintf("Successfully allocated 2 pages for p3\n");

    assert(p3 == p1);
    cprintf("p3 = p1 = %p\n", p3);

    free_pages(p0, 1);
    free_pages(p2, 3);
    free_pages(p3, 2);

    assert(nr_free_pages() == nr_free_pages_record);
    cprintf("free all pages, nr_free_pages() = %d\n", nr_free_pages());

    cprintf("slub_check() succeeded!\n");
}

const struct pmm_manager slub_pmm_manager = {
    .name          = "slub_pmm_manager",
    .init          = slub_init,
    .init_memmap   = slub_init_memmap,
    .alloc_pages   = slub_alloc_pages,
    .free_pages    = slub_free_pages,
    .nr_free_pages = slub_nr_free_pages,
    .check         = slub_check,
};