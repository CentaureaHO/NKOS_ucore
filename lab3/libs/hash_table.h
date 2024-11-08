#ifndef __LIBS_HASH_TABLE_H__
#define __LIBS_HASH_TABLE_H__

#include <defs.h>
#include <list.h>

typedef struct hashtable_entry
{
    struct hashtable_entry* next;
    uintptr_t               key;
} hashtable_entry_t;

typedef struct hashtable
{
    size_t              size;
    hashtable_entry_t** buckets;
} hashtable_t;

typedef size_t (*hash_func_t)(uintptr_t key);

void               hashtable_init(hashtable_t* ht, size_t size, hashtable_entry_t** buckets);
void               hashtable_insert(hashtable_t* ht, hashtable_entry_t* entry, hash_func_t hash_func);
hashtable_entry_t* hashtable_get(hashtable_t* ht, uintptr_t key, hash_func_t hash_func);
void               hashtable_remove(hashtable_t* ht, hashtable_entry_t* entry, hash_func_t hash_func);
void               hashtable_cleanup(hashtable_t* ht);

#endif