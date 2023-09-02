OUTPUT_FORMAT(elf32-i386)
ENTRY(start)
phys = 0x00100000;
virt = 0xC0000000;
SECTIONS
{
  . = phys;
  .multiboot.data : {
    *(.multiboot.data)
  }

  .multiboot.text : {
    *(.multiboot.text)
  }

  . += virt;
  .text ALIGN (4K) : AT (ADDR (.text) - virt) {
    code = .;
    *(.text)
    *(.rodata*)
  }
  .data ALIGN (4K) : AT (ADDR (.data) - virt)
  {
    data = .;
    *(.data)
  }
  .bss ALIGN (4K) : AT (ADDR (.bss) - virt)
  {
    bss = .;
    *(.bss)
  }
  end = .;
}
