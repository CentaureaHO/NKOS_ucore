#ifndef __KERN_MM_BUDDY_SYSTEM_PMM_H__
#define __KERN_MM_BUDDY_SYSTEM_PMM_H__

#include <pmm.h>
// 声明伙伴系统内存管理器的外部变量
extern const struct pmm_manager buddy_system_pmm_manager;

static void         buddy_system_init(void);
static void         buddy_system_init_memmap(struct Page* base, size_t n);
static struct Page* buddy_system_alloc_pages(size_t n);
static void         buddy_system_free_pages(struct Page* base, size_t n);
static size_t       buddy_system_nr_free_pages(void);
static void         buddy_system_check(void);

#endif /* ! __KERN_MM_BUDDY_SYSTEM_PMM_H__ */
