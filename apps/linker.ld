OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

USER_BASE = 0x08048000;

SECTIONS
{
    . = USER_BASE;

    .text : ALIGN(4K)
    {
        *(.text._start)
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
