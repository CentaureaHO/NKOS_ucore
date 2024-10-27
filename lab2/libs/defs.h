#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__

#ifndef NULL
#define NULL ((void*)0)
#endif

#define __always_inline inline __attribute__((always_inline))
#define __noinline __attribute__((noinline))
#define __noreturn __attribute__((noreturn))

/* Represents true-or-false values */
typedef int bool;

/* Explicitly-sized versions of integer types */
typedef char               int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

/* Add fast types */
typedef signed char int_fast8_t;
typedef short       int_fast16_t;
typedef long        int_fast32_t;
typedef long long   int_fast64_t;

typedef unsigned char      uint_fast8_t;
typedef unsigned short     uint_fast16_t;
typedef unsigned long      uint_fast32_t;
typedef unsigned long long uint_fast64_t;

/* *
 * Pointers and addresses are 64 bits long.
 * We use pointer types to represent addresses,
 * uintptr_t to represent the numerical values of addresses.
 * */
typedef int64_t  intptr_t;
typedef uint64_t uintptr_t;

/* size_t is used for memory object sizes */
typedef uintptr_t size_t;

/* used for page numbers */
typedef size_t ppn_t;

/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
/*
    (size_t)(a): 强制转换为size_t
    (size_t)(a) % (n): 取模
    __a - __a % (n): 取整，减去余数后可以保证得到n的整数倍
    (typeof(a))(__a - __a % (n)): 强制转换为a的类型
*/
#define ROUNDDOWN(a, n)               \
    ({                                \
        size_t __a = (size_t)(a);     \
        (typeof(a))(__a - __a % (n)); \
    })

/* Round up to the nearest multiple of n */
/*
    a + __n - 1: 加上n-1，超过上一层的n的整数倍
    ROUNDDOWN(a + __n - 1, __n): 向下取整，得到n的往上取整的整数倍
    如果a本来就是n的整数倍，向下取整后还是a
*/
#define ROUNDUP(a, n)                                       \
    ({                                                      \
        size_t __n = (size_t)(n);                           \
        (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n)); \
    })

/* Return the offset of 'member' relative to the beginning of a struct type */
/*
    (type*)0: 指向地址0的type指针
    (type*)0->member: 首地址为0的type中member
    &((type*)0)->member: 取地址
    (size_t)(&((type*)0)->member): 强制转换为size_t
*/
#define offsetof(type, member) ((size_t)(&((type*)0)->member))

/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
/*
    根据member地址取结构体首地址
*/
#define to_struct(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#endif /* !__LIBS_DEFS_H__ */