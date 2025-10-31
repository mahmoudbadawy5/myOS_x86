OUTPUT_FORMAT("binary")
ENTRY(main)

SECTIONS
{
    . = 0x40000000;
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