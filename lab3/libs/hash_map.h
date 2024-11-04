#ifndef __LIBS_HASH_MAP_H__
#define __LIBS_HASH_MAP_H__

#include <pmm.h>
#include <defs.h>
#include <list.h>

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
    list_entry_t* buckets;
    size_t        bucket_count;
    size_t (*hash_func)(const void* key, size_t key_size);
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size);
} hashmap_t;

static void hashmap_init(hashmap_t* map, size_t bucket_count, size_t (*hash_func)(const void* key, size_t key_size),
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size)) __attribute__((always_inline));
static void hashmap_insert(hashmap_t* map, void* key, size_t key_size, void* value, size_t value_size)
    __attribute__((always_inline));
static hashmap_entry_t* hashmap_find(hashmap_t* map, void* key, size_t key_size) __attribute__((always_inline));
static void             hashmap_remove(hashmap_t* map, void* key, size_t key_size) __attribute__((always_inline));
static void             hashmap_free(hashmap_t* map) __attribute__((always_inline));

static size_t default_hash(const void* key, size_t key_size);
static int    default_key_cmp(const void* key1, const void* key2, size_t key_size);

static inline void hashmap_init(hashmap_t* map, size_t bucket_count,
    size_t (*hash_func)(const void* key, size_t key_size),
    int (*key_cmp)(const void* key1, const void* key2, size_t key_size))
{
    map->buckets      = kmalloc(bucket_count * sizeof(list_entry_t));
    map->bucket_count = bucket_count;
    map->hash_func    = hash_func;
    map->key_cmp      = key_cmp;
    for (size_t i = 0; i < bucket_count; ++i) { list_init(&map->buckets[i]); }
}

static inline void hashmap_insert(hashmap_t* map, void* key, size_t key_size, void* value, size_t value_size)
{
    size_t        bucket_idx = map->hash_func(key, key_size) % map->bucket_count;
    list_entry_t* bucket     = &map->buckets[bucket_idx];

    hashmap_entry_t* new_entry = kmalloc(sizeof(hashmap_entry_t));
    new_entry->key_size        = key_size;
    new_entry->value_size      = value_size;

    new_entry->key = kmalloc(key_size);
    memcpy(new_entry->key, key, key_size);

    new_entry->value = kmalloc(value_size);
    memcpy(new_entry->value, value, value_size);

    list_add_after(bucket, &new_entry->hash_link);
}

static inline hashmap_entry_t* hashmap_find(hashmap_t* map, void* key, size_t key_size)
{
    size_t        bucket_idx = map->hash_func(key, key_size) % map->bucket_count;
    list_entry_t* bucket     = &map->buckets[bucket_idx];

    for (list_entry_t* e = list_next(bucket); e != bucket; e = list_next(e))
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
        kfree(entry->key, entry->key_size);
        kfree(entry->value, entry->value_size);
        kfree(entry, sizeof(hashmap_entry_t));
    }
}

static inline void hashmap_free(hashmap_t* map)
{
    for (size_t i = 0; i < map->bucket_count; ++i)
    {
        list_entry_t* bucket = &map->buckets[i];
        while (!list_empty(bucket))
        {
            list_entry_t* e = list_next(bucket);
            list_del(e);

            hashmap_entry_t* entry = (hashmap_entry_t*)((char*)e - offsetof(hashmap_entry_t, hash_link));
            kfree(entry->key, entry->key_size);
            kfree(entry->value, entry->value_size);
            kfree(entry, sizeof(hashmap_entry_t));
        }
    }
    kfree(map->buckets, map->bucket_count * sizeof(list_entry_t));
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
