#include <stdio.h>
#include "kern/mm/slub.h"

// 计算空闲块链表长度
int slobfree_len1() {
    int len = 0;
    for (slob_t *curr = slobfree->next; curr != slobfree; curr = curr->next) {
        len++;
    }
    return len;
}

// 进行 SLUB 分配器的检查
void slub_check1() {
    printf("slub check begin\n");
    printf("slobfree len: %d\n", slobfree_len1());

    void *p1 = slub_alloc(4096);
    printf("slobfree len: %d\n", slobfree_len1());

    void *p2 = slub_alloc(2);
    void *p3 = slub_alloc(2);
    printf("slobfree len: %d\n", slobfree_len1());

    slub_free(p2);
    printf("slobfree len: %d\n", slobfree_len1());

    slub_free(p3);
    printf("slobfree len: %d\n", slobfree_len1());

    printf("slub check end\n");
}

int main() {
    // 初始化 SLUB 分配器
    slub_init();

    // 执行检查逻辑
    slub_check1();

    return 0;
}

