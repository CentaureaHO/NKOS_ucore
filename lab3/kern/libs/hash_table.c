#include <hash_table.h>

void hashtable_init(hashtable_t* ht, size_t size, hashtable_entry_t** buckets)
{
    ht->size    = size;
    ht->buckets = buckets;
    for (size_t i = 0; i < size; ++i) ht->buckets[i] = NULL;
}

void hashtable_insert(hashtable_t* ht, hashtable_entry_t* entry, hash_func_t hash_func)
{
    size_t index = hash_func(entry->key);
    index        = index % ht->size;

    entry->next        = ht->buckets[index];
    ht->buckets[index] = entry;
}

hashtable_entry_t* hashtable_get(hashtable_t* ht, uintptr_t key, hash_func_t hash_func)
{
    size_t index = hash_func(key);
    index        = index % ht->size;

    hashtable_entry_t* entry = ht->buckets[index];
    while (entry != NULL)
    {
        if (entry->key == key) return entry;
        entry = entry->next;
    }
    return NULL;
}

void hashtable_remove(hashtable_t* ht, hashtable_entry_t* entry, hash_func_t hash_func)
{
    size_t index = hash_func(entry->key);
    index        = index % ht->size;

    hashtable_entry_t** pprev = &ht->buckets[index];
    hashtable_entry_t*  curr  = ht->buckets[index];
    while (curr != NULL)
    {
        if (curr == entry)
        {
            *pprev     = curr->next;
            curr->next = NULL;
            return;
        }
        pprev = &curr->next;
        curr  = curr->next;
    }
}

void hashtable_cleanup(hashtable_t* ht)
{
    for (size_t i = 0; i < ht->size; ++i) ht->buckets[i] = NULL;
}