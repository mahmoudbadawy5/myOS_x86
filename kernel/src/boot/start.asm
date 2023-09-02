; This is the kernel's entry point. We could either call main here,
; or we can use this to setup the stack or other nice stuff, like
; perhaps setting up the GDT and segments. Please note that interrupts
; are disabled at this point: More on interrupts later!
[BITS 32]

; This part MUST be 4byte aligned, so we solve that issue using 'ALIGN 4'
SECTION .multiboot.data
ALIGN 4
mboot:
    ; Multiboot macros to make a few lines later more readable
    MULTIBOOT_PAGE_ALIGN	equ 1<<0
    MULTIBOOT_MEMORY_INFO	equ 1<<1
    ;MULTIBOOT_AOUT_KLUDGE	equ 1<<16
    MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO ;| MULTIBOOT_AOUT_KLUDGE
    MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    EXTERN code, bss, end

    ; This is the GRUB Multiboot header. A boot signature
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM
    
    ; AOUT kludge - must be physical addresses. Make a note of these:
    ; The linker script fills in the data for these ones!
    dd mboot
    dd code
    dd bss
    dd end
    dd start

KERNEL_VIRTUAL_BASE equ 0xC0000000
KERNEL_PAGE_NUMBER equ (KERNEL_VIRTUAL_BASE >> 22)

ALIGN 0x1000

boot_page_table:
    times (1024) dd 0
    
boot_page_directory:
    dd 3                    ; Identity map at first
    times (KERNEL_PAGE_NUMBER-1) dd 0           ; All other pages are empty
    dd 3                    ; 3GB page mapped to first 4MB
    times (1024 - KERNEL_PAGE_NUMBER - 1) dd 0  ; All other pages are empty after the kernel page

; start function that sets up paging
SECTION .multiboot.text  progbits alloc exec nowrite align=16
global start
start:

    mov ecx, 1024
    mov edx, 0
loop: ; Setting page table to map to first 4MB
    mov dword [boot_page_table + 4*edx], edx
    shl dword [boot_page_table + 4*edx], 12
    or  dword [boot_page_table + 4*edx], 3
    inc edx
    dec ecx
    jnz loop

    or dword [boot_page_directory], boot_page_table
    or dword [boot_page_directory + 4*KERNEL_PAGE_NUMBER], boot_page_table

    mov [0x000B8000], byte 'A'
    mov [0x000B8001], byte 0x0F


    mov ecx, boot_page_directory
    mov cr3, ecx ; Pointing cr3 to page directory

    mov [0x000B8002], byte 'B'
    mov [0x000B8003], byte 0x0F

    mov ecx, cr0
    or ecx, 0x80000000 ; Setting up paging bit
    mov cr0, ecx ; Paging is set up

    mov [0xC00B8004], byte 'C'
    mov [0xC00B8005], byte 0x0F
    jmp stublet ; jmp to the higher half

; This is an endless loop here. Make a note of this: Later on, we
; will insert an 'extern _main', followed by 'call _main', right
; before the 'jmp $'.
SECTION .text
stublet:
    mov [0xC00B8006], byte 'D'
    mov [0xC00B8007], byte 0x0F
    mov dword [boot_page_directory], 0
    invlpg [0] ; invalidate first page as it's not needed any more
    mov esp, _sys_stack
    add ebx, KERNEL_VIRTUAL_BASE ; converting multiboot struct address to virtual address
    push ebx
    push eax
    extern kmain
    call kmain
    jmp $


; Shortly we will add code for loading the GDT right here!


; In just a few pages in this tutorial, we will add our Interrupt
; Service Routines (ISRs) right here!



; Here is the definition of our BSS section. Right now, we'll use
; it just to store the stack. Remember that a stack actually grows
; downwards, so we declare the size of the data before declaring
; the identifier '_sys_stack'
SECTION .bss
    resb 8192*2               ; This reserves 8KBytes of memory here
_sys_stack: