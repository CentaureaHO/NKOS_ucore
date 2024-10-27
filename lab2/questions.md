# Questions

1. 让OS获知可用物理内存范围：
   1. libs/sbi.h 11 sbi_query_memory，通过opensbi反复获取每个内存块信息，直到返回错误信息；
   2. 顺序访问内存地址，直到访问到非法地址（可以跳着访问，出问题了再回头）

2. dram范围：
   1. 起始地址：kern/mm/memlayout.h 16 #define KERNEL_BEGIN_PADDR 0x80200000
   2. 尾地址：kern/mm/memlayout.h 14 #define PHYSICAL_MEMORY_END 0x88000000，实际使用时-1

3. ddd
