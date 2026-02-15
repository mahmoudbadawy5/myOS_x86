OUTPUT_FORMAT("binary")
ENTRY(main)

USER_BASE = 0x40000000;

SECTIONS
{
    . = USER_BASE;
    .text :
    {
        *(.text*);
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