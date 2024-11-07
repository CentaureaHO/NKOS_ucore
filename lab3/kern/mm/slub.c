#include <slub.h>
#include <list.h>
#include <defs.h>
#include <string.h>
#include <stdio.h>

#define USING_COUPLED_SLAB

#define CACHE_NUM 8
static const size_t cache_size_min = 16;
static const size_t cache_size_max = cache_size_min << (CACHE_NUM - 1);

#ifdef USING_COUPLED_SLAB
#define DESTROY_SLABS(cache_ptr, list)                                                   \
    {                                                                                    \
        list_entry_t* le = list_next(list);                                              \
        while (le != list)                                                               \
        {                                                                                \
            list_entry_t* temp = le;                                                     \
            le                 = list_next(le);                                          \
            _slab_destroy((cache_ptr), (slab_t*)le2page((struct Page*)temp, page_link)); \
        }                                                                                \
    }
#else
#define DESTROY_SLABS(cache_ptr, list)                     \
    {                                                      \
        list_entry_t* le = list_next(list);                \
        while (le != list)                                 \
        {                                                  \
            list_entry_t* temp = le;                       \
            le                 = list_next(le);            \
            slab_t* slab       = le2slab(temp, slab_link); \
            _slab_destroy((cache_ptr), slab);              \
        }                                                  \
    }

#define le2slab(le, member) to_struct((le), slab_t, member)
#endif

typedef struct slub_cache
{
    list_entry_t slabs_full;
    list_entry_t slabs_partial;
    list_entry_t slabs_free;

    uint16_t object_size;
    uint16_t objects_per_slab;

    list_entry_t cache_link;
} slub_cache_t;

typedef struct slab
{
    int           ref;
    slub_cache_t* cache_ptr;

#ifdef USING_COUPLED_SLAB
    uint_t ph_visited;
#endif

    uint16_t     in_use;
    int16_t      free_index;
    list_entry_t slab_link;

#ifdef USING_COUPLED_SLAB
    list_entry_t ph_pra_page_link;
    uintptr_t    ph_pra_vaddr;
#else
    struct Page* page;
#endif
} slab_t;

static list_entry_t  _cache_registry;
static slub_cache_t  _cache_pool;
static slub_cache_t* _caches[CACHE_NUM];

static void* _cache_alloc(slub_cache_t* cache_ptr);
static void  _cache_free(slub_cache_t* cache_ptr, void* obj_ptr);

static void* _cache_expand(slub_cache_t* cache_ptr)
{
    struct Page* page = alloc_page();
    if (!page) return NULL;

    void* kva = page2kva(page);
#ifdef USING_COUPLED_SLAB
    slab_t* slab = (slab_t*)page;
#else
    slab_t* slab = (slab_t*)kva;
    slab->page   = page;
#endif

    slab->cache_ptr  = cache_ptr;
    slab->in_use     = 0;
    slab->free_index = 0;

#ifdef USING_COUPLED_SLAB
    cache_ptr->objects_per_slab = PGSIZE / (sizeof(int16_t) + cache_ptr->object_size);
    int16_t* idx_buffer         = kva;
#else
    cache_ptr->objects_per_slab = (PGSIZE - sizeof(slab_t)) / (sizeof(int16_t) + cache_ptr->object_size);
    int16_t* idx_buffer         = (int16_t*)((char*)kva + sizeof(slab_t));
#endif

    for (size_t i = 1; i < cache_ptr->objects_per_slab; ++i) idx_buffer[i - 1] = i;
    idx_buffer[cache_ptr->objects_per_slab - 1] = -1;
    list_add(&(cache_ptr->slabs_free), &(slab->slab_link));
    return slab;
}

static void _slab_destroy(slub_cache_t* cache_ptr, slab_t* slab)
{
    (void)cache_ptr;
#ifdef USING_COUPLED_SLAB
    struct Page* page = (struct Page*)slab;
    page->property    = 0;
    page->flags       = 0;
    list_del(&(page->page_link));
#else
    struct Page* page = slab->page;
    list_del(&(slab->slab_link));
#endif
    free_page(page);
}

static size_t _sized_index(size_t size)
{
    size_t rsize = ROUNDUP(size, 2);
    if (rsize < cache_size_min) rsize = cache_size_min;

    size_t index = 0;
    for (int t = rsize / 32; t; t /= 2) ++index;
    return index;
}

static slub_cache_t* _cache_create(size_t size)
{
    assert(size <= (PGSIZE - 2));

    slub_cache_t* cache_ptr = (slub_cache_t*)_cache_alloc(&_cache_pool);
    if (!cache_ptr) return NULL;
    cache_ptr->object_size = size;

    list_init(&(cache_ptr->slabs_full));
    list_init(&(cache_ptr->slabs_partial));
    list_init(&(cache_ptr->slabs_free));
    list_add(&(_cache_registry), &(cache_ptr->cache_link));
    return cache_ptr;
}

void _cache_destroy(slub_cache_t* cache_ptr)
{
    DESTROY_SLABS(cache_ptr, &(cache_ptr->slabs_full));
    DESTROY_SLABS(cache_ptr, &(cache_ptr->slabs_partial));
    DESTROY_SLABS(cache_ptr, &(cache_ptr->slabs_free));

    _cache_free(&_cache_pool, cache_ptr);
}

static void* _cache_alloc(slub_cache_t* cache_ptr)
{
    list_entry_t* le = NULL;
    if (!list_empty(&(cache_ptr->slabs_partial)))
        le = list_next(&(cache_ptr->slabs_partial));
    else
    {
        if (list_empty(&(cache_ptr->slabs_free)) && _cache_expand(cache_ptr) == NULL) return NULL;
        le = list_next(&(cache_ptr->slabs_free));
    }

#ifdef USING_COUPLED_SLAB
    slab_t*  slab       = (slab_t*)le2page((struct Page*)le, page_link);
    void*    kva        = page2kva((struct Page*)slab);
    int16_t* idx_buffer = kva;
#else
    slab_t*  slab       = le2slab(le, slab_link);
    void*    kva        = (void*)slab;
    int16_t* idx_buffer = (int16_t*)((char*)kva + sizeof(slab_t));
#endif

    void* buf     = (void*)(idx_buffer + cache_ptr->objects_per_slab);
    void* obj_ptr = buf + slab->free_index * cache_ptr->object_size;

    ++slab->in_use;
    slab->free_index = idx_buffer[slab->free_index];

    list_del(le);

    if (slab->in_use == cache_ptr->objects_per_slab)
        list_add(&(cache_ptr->slabs_full), le);
    else
        list_add(&(cache_ptr->slabs_partial), le);

    return obj_ptr;
}

static void _cache_free(slub_cache_t* cache_ptr, void* obj_ptr)
{
#ifdef USING_COUPLED_SLAB
    void*   kva  = ROUNDDOWN(obj_ptr, PGSIZE);
    slab_t* slab = (slab_t*)&pages[(kva - page2kva(pages)) / PGSIZE];

    int16_t* idx_buffer = kva;
    void*    buf        = idx_buffer + cache_ptr->objects_per_slab;
    int      offset     = (obj_ptr - buf) / cache_ptr->object_size;

    list_del(&(slab->slab_link));
    idx_buffer[offset] = slab->free_index;
    slab->in_use--;
    slab->free_index = offset;

    if (slab->in_use == 0)
        list_add(&(cache_ptr->slabs_free), &(slab->slab_link));
    else
        list_add(&(cache_ptr->slabs_partial), &(slab->slab_link));
#else
    void*   kva  = ROUNDDOWN(obj_ptr, PGSIZE);
    slab_t* slab = (slab_t*)kva;

    int16_t* idx_buffer = (int16_t*)((char*)kva + sizeof(slab_t));
    void*    buf        = (void*)(idx_buffer + cache_ptr->objects_per_slab);
    int      offset     = (obj_ptr - buf) / cache_ptr->object_size;

    idx_buffer[offset] = slab->free_index;
    slab->free_index   = offset;
    --slab->in_use;

    list_del(&(slab->slab_link));

    if (slab->in_use == 0)
        list_add(&(cache_ptr->slabs_free), &(slab->slab_link));
    else
        list_add(&(cache_ptr->slabs_partial), &(slab->slab_link));
#endif
}

static int _cache_shrink(slub_cache_t* cache_ptr)
{
#ifdef USING_COUPLED_SLAB
    int           count = 0;
    list_entry_t* le    = list_next(&(cache_ptr->slabs_free));
    while (le != &(cache_ptr->slabs_free))
    {
        list_entry_t* temp = le;
        le                 = list_next(le);
        _slab_destroy(cache_ptr, (slab_t*)le2page((struct Page*)temp, page_link));
        ++count;
    }
#else
    int           count = 0;
    list_entry_t* le    = list_next(&(cache_ptr->slabs_free));
    while (le != &(cache_ptr->slabs_free))
    {
        list_entry_t* temp = le;
        le                 = list_next(le);
        slab_t* slab       = le2slab(temp, slab_link);
        _slab_destroy(cache_ptr, slab);
        ++count;
    }
#endif
    return count;
}

void slub_allocator_init()
{
#ifdef USING_COUPLED_SLAB
    _cache_pool.object_size      = sizeof(slub_cache_t);
    _cache_pool.objects_per_slab = PGSIZE / (sizeof(int16_t) + sizeof(slub_cache_t));

    list_init(&(_cache_pool.slabs_full));
    list_init(&(_cache_pool.slabs_partial));
    list_init(&(_cache_pool.slabs_free));
    list_init(&(_cache_registry));

    list_add(&(_cache_registry), &(_cache_pool.cache_link));

    for (size_t i = 0; i < CACHE_NUM; ++i) _caches[i] = _cache_create(cache_size_min << i);
#else
    _cache_pool.object_size = sizeof(slub_cache_t);

    list_init(&(_cache_pool.slabs_full));
    list_init(&(_cache_pool.slabs_partial));
    list_init(&(_cache_pool.slabs_free));
    list_init(&(_cache_registry));

    list_add(&(_cache_registry), &(_cache_pool.cache_link));

    cprintf("slub allocator initialized\n");

    for (size_t i = 0; i < CACHE_NUM; ++i) _caches[i] = _cache_create(cache_size_min << i);

    cprintf("slub caches created\n");
#endif
}

void* slub_malloc(size_t size)
{
    assert(size <= cache_size_max);
    return _cache_alloc(_caches[_sized_index(size)]);
}

int slub_free(void* obj_ptr)
{
    if (obj_ptr == NULL) return -1;

    void* kva = ROUNDDOWN(obj_ptr, PGSIZE);
#ifdef USING_COUPLED_SLAB
    slab_t* slab = (slab_t*)&pages[(kva - page2kva(pages)) / PGSIZE];
    if (slab == NULL || slab->in_use == 0) return -1;
    _cache_free(slab->cache_ptr, obj_ptr);
#else
    slab_t* slab = (slab_t*)kva;
    if (slab == NULL || slab->in_use == 0) return -1;
    _cache_free(slab->cache_ptr, obj_ptr);
#endif

    return 0;
}

int slub_reap()
{
    int           count = 0;
    list_entry_t* le    = &(_cache_registry);
    while ((le = list_next(le)) != &(_cache_registry)) count += _cache_shrink(to_struct(le, slub_cache_t, cache_link));
    return count;
}

#ifdef USING_COUPLED_SLAB
#undef USING_COUPLED_SLAB
#endif