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

typedef struct hashmap_iterator
{
    hashmap_base_t* iter_map;
    size_t          bucket_index;
    list_entry_t*   list_pos;
} hashmap_iterator_t;

#define hashmap(key_type, data_type)          \
    struct                                    \
    {                                         \
        hashmap_base_t base;                  \
        struct                                \
        {                                     \
            key_type*  key;                   \
            data_type* data;                  \
            struct                            \
            {                                 \
                hashmap_base_t* iter_map;     \
                size_t          bucket_index; \
                list_entry_t*   list_pos;     \
                struct                        \
                {                             \
                    const key_type* t_key;    \
                    data_type*      t_data;   \
                } iter_types[0];              \
            } t_iterator;                     \
        } map_type[0];                        \
    }

#define hashmap_iter(hm)                               \
    struct                                             \
    {                                                  \
        hashmap_iterator_t iter;                       \
        struct                                         \
        {                                              \
            const typeof(*(hm).map_type->key)* t_key;  \
            typeof(*(hm).map_type->data)*      t_data; \
        } iter_types[0];                               \
    }

#define hashmap_init(hm, bucket_size, hash_func, cmp_func)  \
    {                                                       \
        hashmap_base_init(&(hm).base,                       \
            bucket_size,                                    \
            (size_t(*)(const void*))(hash_func),            \
            (int (*)(const void*, const void*))(cmp_func)); \
    }

#define hashmap_put(hm, key, data) hashmap_base_put(&(hm).base, (void*)(key), (void*)(data))

#define hashmap_get(hm, key) ((typeof(*(hm).map_type->data)*)hashmap_base_get(&(hm).base, (const void*)(key)))

#define hashmap_remove(hm, key) hashmap_base_remove(&(hm).base, (const void*)(key))

#define hashmap_destroy(hm) hashmap_base_destroy(&(hm).base)

#define hashmap_iter_init(hm, iter) hashmap_base_iter_init(&(iter.iter), &(hm).base)

#define hashmap_iter_valid(iter) hashmap_base_iter_valid(&(iter.iter))

#define hashmap_iter_next(iter) hashmap_base_iter_next(&(iter.iter))

#define hashmap_iter_get_key(iter) ((typeof(*(iter).iter_types->t_key)*)hashmap_base_iter_get_key(&(iter.iter)))

#define hashmap_iter_get_data(iter) ((typeof(*(iter).iter_types->t_data)*)hashmap_base_iter_get_data(&(iter.iter)))

#define hashmap_iter_remove(iter) hashmap_base_iter_remove(&(iter.iter))

void  hashmap_base_init(hashmap_base_t* hb, size_t bucket_size, size_t (*hash_func)(const void*),
     int (*compare_func)(const void*, const void*));
int   hashmap_base_put(hashmap_base_t* hb, void* key, void* data);
void* hashmap_base_get(hashmap_base_t* hb, const void* key);
int   hashmap_base_remove(hashmap_base_t* hb, const void* key);
void  hashmap_base_destroy(hashmap_base_t* hb);

void  hashmap_base_iter_init(hashmap_iterator_t* iter, hashmap_base_t* hb);
int   hashmap_base_iter_valid(hashmap_iterator_t* iter);
void  hashmap_base_iter_next(hashmap_iterator_t* iter);
void* hashmap_base_iter_get_key(hashmap_iterator_t* iter);
void* hashmap_base_iter_get_data(hashmap_iterator_t* iter);
void  hashmap_base_iter_remove(hashmap_iterator_t* iter);

#endif /* !__LIBS_HASH_MAP_H__ */