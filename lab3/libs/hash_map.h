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
    size_t (*hash_func)(const void*);
    int (*cmp_func)(const void*, const void*);
} hashmap_base_t;

#define hashmap(key_type, data_type) \
    struct                           \
    {                                \
        hashmap_base_t base;         \
        struct                       \
        {                            \
            key_type*  key;          \
            data_type* data;         \
        } map_type[0];               \
    }

#define hashmap_init(hm, bucket_size, hash_func, cmp_func)  \
    do {                                                    \
        hashmap_base_init(&(hm).base,                       \
            bucket_size,                                    \
            (size_t(*)(const void*))(hash_func),            \
            (int (*)(const void*, const void*))(cmp_func)); \
    } while (0)

#define hashmap_put(hm, key, data) hashmap_base_put(&(hm).base, (void*)(key), (void*)(data))

#define hashmap_get(hm, key) ((typeof(*(hm).map_type->data)*)hashmap_base_get(&(hm).base, (const void*)(key)))

#define hashmap_remove(hm, key) hashmap_base_remove(&(hm).base, (const void*)(key))

#define hashmap_destroy(hm) hashmap_base_destroy(&(hm).base)

void  hashmap_base_init(hashmap_base_t* hb, size_t bucket_size, size_t (*hash_func)(const void*),
     int (*compare_func)(const void*, const void*));
int   hashmap_base_put(hashmap_base_t* hb, void* key, void* data);
void* hashmap_base_get(hashmap_base_t* hb, const void* key);
int   hashmap_base_remove(hashmap_base_t* hb, const void* key);
void  hashmap_base_destroy(hashmap_base_t* hb);

#endif /* !__LIBS_HASH_MAP_H__ */