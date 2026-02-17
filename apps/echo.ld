OUTPUT_FORMAT("binary")
ENTRY(_start)

USER_BASE = 0x40000000;

SECTIONS
{
    . = USER_BASE;
    .text :
    {
        *(.text._start);
        *(.text);
    }
    .data :
    {
        *(.data*);
    }
    .rodata :
    {
        *(.rodata*);
    }
    .bss :
    {
        *(.bss*);
    }
}