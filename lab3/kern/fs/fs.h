#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <mmu.h>

#define SECTSIZE 512                    // sector size，扇区大小
#define PAGE_NSECT (PGSIZE / SECTSIZE)  // 每页所需扇区数

#define SWAP_DEV_NO 1

#endif /* !__KERN_FS_FS_H__ */
