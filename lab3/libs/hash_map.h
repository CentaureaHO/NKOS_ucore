#ifndef __LIBS_HASH_MAP_H__
#define __LIBS_HASH_MAP_H__

#include <defs.h>
#include <list.h>
#include <slub.h>

typedef struct hashmap_entry
{
    void*        key;
    void*        data;
    list_entry_t hash_link;
} hashmap_entry_t;

typedef struct hashmap_base
{
    size_t        bucket_size;
    list_entry_t* table;
    // size_t (*hash_func)(const void*);
    // int (*cmp_func)(const void*, const void*);
    // void* (*key_dup)(const void*);
    // void (*key_free)(void*);
} hashmap_base_t;

#define hashmap(key_type, data_type)                           \
    struct                                                     \
    {                                                          \
        hashmap_base_t base;                                   \
        struct                                                 \
        {                                                      \
            key_type* key;                               \
            data_type*      data;                              \
            size_t (*hash_func)(const key_type*);              \
            int (*cmp_func)(const key_type*, const key_type*); \
        } map_type[0];                                           \
    }

#endif /* !__LIBS_HASH_MAP_H__ */