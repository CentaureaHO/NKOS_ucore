#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */

// 一个线性地址 'la' 的结构分为以下四部分：
//
// +--------9-------+-------9--------+-------9--------+---------12----------+
// | 页目录索引1    | 页目录索引2    |   页表索引     | 页内偏移             |
// +----------------+----------------+----------------+---------------------+
//   \-- PDX1(la) --/ \-- PDX0(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//   \-------------------PPN(la)----------------------/
//
// 宏 PDX1, PDX0, PTX, PGOFF 和 PPN 用于如上所示分解线性地址。
// 若想从 PDX(la), PTX(la), 和 PGOFF(la) 构造线性地址 la，
// 使用 PGADDR(PDX(la), PTX(la), PGOFF(la))。

// RISC-V 使用 39 位虚拟地址访问 56 位物理地址！
// Sv39 虚拟地址：
// +----9----+----9---+----9---+---12--+
// |  VPN[2] | VPN[1] | VPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 物理地址：
// +----26---+----9---+----9---+---12--+
// |  PPN[2] | PPN[1] | PPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 页表项：
// +----26---+----9---+----9---+---2----+-------8-------+
// |  PPN[2] | PPN[1] | PPN[0] |保留位 |D|A|G|U|X|W|R|V|
// +---------+----+---+--------+--------+---------------+

// 页目录索引
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

// 页表索引
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)

// 地址的页号字段
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// 页内偏移
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// 从索引和偏移构造线性地址
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT | (d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))

// 页表或页目录项的地址
#define PTE_ADDR(pte) (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PDE_ADDR(pde) PTE_ADDR(pde)

/* 页目录和页表的常量 */
#define NPDEENTRY 512  // 每个页目录的页目录项数
#define NPTEENTRY 512  // 每个页表的页表项数

#define PGSIZE 4096                  // 每页映射的字节数
#define PGSHIFT 12                   // log2(PGSIZE)
#define PTSIZE (PGSIZE * NPTEENTRY)  // 每个页目录项映射的字节数
#define PTSHIFT 21                   // log2(PTSIZE)

#define PTXSHIFT 12       // 线性地址中 PTX 的偏移
#define PDX0SHIFT 21      // 线性地址中 PDX0 的偏移
#define PDX1SHIFT 30      // 线性地址中 PDX1 的偏移
#define PTE_PPN_SHIFT 10  // 物理地址中 PPN 的偏移

// 页表项 (PTE) 字段
#define PTE_V 0x001     // 有效
#define PTE_R 0x002     // 可读
#define PTE_W 0x004     // 可写
#define PTE_X 0x008     // 可执行
#define PTE_U 0x010     // 用户
#define PTE_G 0x020     // 全局
#define PTE_A 0x040     // 已访问
#define PTE_D 0x080     // 脏页
#define PTE_SOFT 0x300  // 为软件保留

#define PAGE_TABLE_DIR (PTE_V)
#define READ_ONLY (PTE_R | PTE_V)
#define READ_WRITE (PTE_R | PTE_W | PTE_V)
#define EXEC_ONLY (PTE_X | PTE_V)
#define READ_EXEC (PTE_R | PTE_X | PTE_V)
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V)

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)

#endif /* !__KERN_MM_MMU_H__ */
