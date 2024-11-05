#ifndef __LIBS_HASH_MAP_H__
#define __LIBS_HASH_MAP_H__

#include <defs.h>
#include <list.h>
#include <slub.h>

#define BUCKET_COUNT 8
#define MAX_ENTRIES 32

typedef struct
{
    void*        key;
    size_t       key_size;
    void*        value;
    size_t       value_size;
    list_entry_t hash_link;
} hashmap_entry_t;

typedef struct
{
    list_entry_t buckets[BUCKET_COUNT];
    size_t       entry_count;
    size_t (*hash_func)(const void* key, size_t key_size);
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size);
    hashmap_entry_t entries[MAX_ENTRIES];
} hashmap_t;

static void hashmap_init(hashmap_t* map, size_t (*hash_func)(const void* key, size_t key_size),
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size)) __attribute__((always_inline));
static int  hashmap_insert(hashmap_t* map, void* key, size_t key_size, void* value, size_t value_size)
    __attribute__((always_inline));
static hashmap_entry_t* hashmap_find(hashmap_t* map, void* key, size_t key_size) __attribute__((always_inline));
static void             hashmap_remove(hashmap_t* map, void* key, size_t key_size) __attribute__((always_inline));
static void             hashmap_clear(hashmap_t* map) __attribute__((always_inline));

static size_t default_hash(const void* key, size_t key_size);
static int    default_key_cmp(const void* key1, const void* key2, size_t key_size);

static inline void hashmap_init(hashmap_t* map, size_t (*hash_func)(const void* key, size_t key_size),
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size))
{
    map->entry_count = 0;
    map->hash_func   = hash_func;
    map->key_cmp     = key_cmp;
    for (size_t i = 0; i < BUCKET_COUNT; ++i) { list_init(&map->buckets[i]); }
}

static inline int hashmap_insert(hashmap_t* map, void* key, size_t key_size, void* value, size_t value_size)
{
    if (map->entry_count >= MAX_ENTRIES) return -1;

    size_t        bucket_idx = map->hash_func(key, key_size) % BUCKET_COUNT;
    list_entry_t* bucket     = &map->buckets[bucket_idx];

    hashmap_entry_t* new_entry = &map->entries[map->entry_count++];
    new_entry->key_size        = key_size;
    new_entry->value_size      = value_size;

    new_entry->key   = key;
    new_entry->value = value;

    list_add_after(bucket, &new_entry->hash_link);
    return 0;
}

static inline hashmap_entry_t* hashmap_find(hashmap_t* map, void* key, size_t key_size)
{
    size_t        bucket_idx = map->hash_func(key, key_size) % BUCKET_COUNT;
    list_entry_t* bucket     = &map->buckets[bucket_idx];

    list_entry_t* e;
    list_for_each(e, bucket)
    {
        hashmap_entry_t* entry = (hashmap_entry_t*)((char*)e - offsetof(hashmap_entry_t, hash_link));
        if (entry->key_size == key_size && map->key_cmp(entry->key, key, key_size) == 0) { return entry; }
    }
    return NULL;
}

static inline void hashmap_remove(hashmap_t* map, void* key, size_t key_size)
{
    hashmap_entry_t* entry = hashmap_find(map, key, key_size);
    if (entry != NULL)
    {
        list_del(&entry->hash_link);
        entry->key   = NULL;
        entry->value = NULL;
    }
}

static inline void hashmap_clear(hashmap_t* map)
{
    for (size_t i = 0; i < BUCKET_COUNT; ++i)
    {
        list_entry_t* bucket = &map->buckets[i];
        while (!list_empty(bucket))
        {
            list_entry_t* e = list_next(bucket);
            list_del(e);
        }
    }
    map->entry_count = 0;
}

static inline size_t default_hash(const void* key, size_t key_size)
{
    size_t      hash = 0;
    const char* ptr  = key;
    for (size_t i = 0; i < key_size; ++i) hash = hash * 31 + ptr[i];
    return hash;
}

static inline int default_key_cmp(const void* key1, const void* key2, size_t key_size)
{
    return memcmp(key1, key2, key_size);
}

#endif /* !__LIBS_HASH_MAP_H__ */