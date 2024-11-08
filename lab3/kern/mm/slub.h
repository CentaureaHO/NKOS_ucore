#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <pmm.h>
#include <list.h>

#define smalloc(type) ((type*)slub_malloc(sizeof(type)))
#define smalloca(type, size) ((type*)slub_malloc(size * sizeof(type)))
#define sfree(objp)                                              \
    {                                                            \
        if (!objp) panic("slub_free(NULL)\n");                   \
        if (slub_free(objp) != 0) panic("slub_free() failed\n"); \
        objp = NULL;                                             \
    }

void  slub_allocator_init();
void* slub_malloc(size_t size);
int   slub_free(void* objp);
int   slub_reap();

#endif /* !__KERN_MM_SLUB_H__ */