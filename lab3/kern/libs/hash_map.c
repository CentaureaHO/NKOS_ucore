#include <hash_map.h>
#include <slub.h>
#include <assert.h>

void hashmap_base_init(hashmap_base_t* hb, size_t bucket_size, size_t (*hash_func)(const void*),
    int (*compare_func)(const void*, const void*))
{
    hb->bucket_size = bucket_size;
    hb->table       = smalloca(list_entry_t, bucket_size);
    assert(hb->table != NULL);

    for (size_t i = 0; i < bucket_size; ++i) { list_init(&(hb->table[i])); }

    hb->hash_func = hash_func;
    hb->cmp_func  = compare_func;
}

int hashmap_base_put(hashmap_base_t* hb, void* key, void* data)
{
    size_t hash  = hb->hash_func(key);
    size_t index = hash % hb->bucket_size;

    list_entry_t* bucket = &(hb->table[index]);

    list_entry_t* le;
    list_for_each(le, bucket)
    {
        hashmap_entry_t* entry = to_struct(le, hashmap_entry_t, hash_link);
        if (hb->cmp_func(entry->key, key) == 0)
        {
            entry->data = data;
            return 0;
        }
    }

    hashmap_entry_t* new_entry = smalloc(hashmap_entry_t);
    if (!new_entry) return -1;

    new_entry->key  = key;
    new_entry->data = data;
    list_init(&new_entry->hash_link);
    list_add_after(bucket, &new_entry->hash_link);

    return 0;
}

void* hashmap_base_get(hashmap_base_t* hb, const void* key)
{
    size_t hash  = hb->hash_func(key);
    size_t index = hash % hb->bucket_size;

    list_entry_t* bucket = &(hb->table[index]);

    list_entry_t* le;
    list_for_each(le, bucket)
    {
        hashmap_entry_t* entry = to_struct(le, hashmap_entry_t, hash_link);
        if (hb->cmp_func(entry->key, key) == 0) { return entry->data; }
    }

    return NULL;
}

int hashmap_base_remove(hashmap_base_t* hb, const void* key)
{
    size_t hash  = hb->hash_func(key);
    size_t index = hash % hb->bucket_size;

    list_entry_t* bucket = &(hb->table[index]);

    list_entry_t* le;
    list_for_each(le, bucket)
    {
        hashmap_entry_t* entry = to_struct(le, hashmap_entry_t, hash_link);
        if (hb->cmp_func(entry->key, key) == 0)
        {
            list_del(&entry->hash_link);
            sfree(entry);
            return 0;
        }
    }

    return -1;
}

void hashmap_base_destroy(hashmap_base_t* hb)
{
    for (size_t i = 0; i < hb->bucket_size; ++i)
    {
        list_entry_t* bucket = &hb->table[i];
        list_entry_t* le     = list_next(bucket);

        while (le != bucket)
        {
            hashmap_entry_t* entry = to_struct(le, hashmap_entry_t, hash_link);
            le                     = list_next(le);
            list_del(&entry->hash_link);
            sfree(entry);
        }
    }
    sfree(hb->table);
}

void hashmap_base_iter_init(hashmap_iterator_t* iter, hashmap_base_t* hb)
{
    iter->iter_map     = hb;
    iter->bucket_index = 0;
    iter->list_pos     = NULL;

    while (iter->bucket_index < hb->bucket_size)
    {
        list_entry_t* bucket = &hb->table[iter->bucket_index];
        if (!list_empty(bucket))
        {
            iter->list_pos = list_next(bucket);
            if (iter->list_pos != bucket) return;
        }
        iter->bucket_index++;
    }

    iter->list_pos = NULL;
}

int hashmap_base_iter_valid(hashmap_iterator_t* iter) { return iter->list_pos != NULL; }

void hashmap_base_iter_next(hashmap_iterator_t* iter)
{
    if (iter->list_pos == NULL) return;

    list_entry_t* bucket = &iter->iter_map->table[iter->bucket_index];
    iter->list_pos       = list_next(iter->list_pos);

    if (iter->list_pos == bucket)
    {
        iter->bucket_index++;
        while (iter->bucket_index < iter->iter_map->bucket_size)
        {
            bucket = &iter->iter_map->table[iter->bucket_index];
            if (!list_empty(bucket))
            {
                iter->list_pos = list_next(bucket);
                if (iter->list_pos != bucket) return;
            }
            iter->bucket_index++;
        }

        iter->list_pos = NULL;
    }
}

void* hashmap_base_iter_get_key(hashmap_iterator_t* iter)
{
    if (iter->list_pos == NULL) return NULL;
    hashmap_entry_t* entry = to_struct(iter->list_pos, hashmap_entry_t, hash_link);
    return entry->key;
}

void* hashmap_base_iter_get_data(hashmap_iterator_t* iter)
{
    if (iter->list_pos == NULL) return NULL;
    hashmap_entry_t* entry = to_struct(iter->list_pos, hashmap_entry_t, hash_link);
    return entry->data;
}

void hashmap_base_iter_remove(hashmap_iterator_t* iter)
{
    if (iter->list_pos == NULL) return;

    list_entry_t* current = iter->list_pos;

    hashmap_base_iter_next(iter);

    hashmap_entry_t* entry = to_struct(current, hashmap_entry_t, hash_link);
    list_del(current);
    sfree(entry);
}