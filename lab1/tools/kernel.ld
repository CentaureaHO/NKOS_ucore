/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;

    .text : {       /* 起始地址为80200000 */
        *(.text.kern_entry)     /* kern_entry为起始地址往后第一段要链接的程序，因此放置到80200000 */
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);      /* 对齐到4K，随后划分内核栈 */
                            /* 对齐后地址直接划归bootstack */
                            /* 随后往后保留KSATCKSIZE个bytes */
                            /* 将bootstack + KSATCKSIZE / 4的位置划归bootstacktop */
                            /* 中间即为预留的内核栈 */

    /* The data segment */
    .data : {
        *(.data)
        *(.data.*)
    }

    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

    PROVIDE(edata = .);

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .);

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
