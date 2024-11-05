#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <pmm.h>
#include <list.h>

void  slub_allocator_init();
void* slub_malloc(size_t size);
int   slub_free(void* objp);
int   slub_reap();

#endif /* !__KERN_MM_SLUB_H__ */