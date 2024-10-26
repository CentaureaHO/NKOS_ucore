#ifndef SLUB_H
#define SLUB_H

#include "../../libs/defs.h"

// 初始化 SLUB 分配器
void slub_init(void);

// 分配指定大小的内存块
void *slub_alloc(size_t size);

// 释放指定内存块
void slub_free(void *objp);

// 检查分配器状态（如内存泄漏或错误）
void slub_check(void);

#endif /* SLUB_H */

