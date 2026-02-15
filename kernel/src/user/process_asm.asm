[GLOBAL switch_to_process]
[EXTERN current_process]

PCB_OFFSET_FIRST_RUN     equ 8
PCB_OFFSET_KERNEL_STACK  equ 12
PCB_OFFSET_CR3           equ 56

switch_to_process:
    mov eax, [esp + 4]
    mov ecx, [current_process]
    test ecx, ecx
    jz .load_next
    cmp dword [ecx + PCB_OFFSET_FIRST_RUN], 0
    jnz .load_next
    mov [ecx + PCB_OFFSET_KERNEL_STACK], esp
.load_next:
    mov [current_process], eax
    mov ecx, [eax + PCB_OFFSET_CR3]
    mov cr3, ecx
    mov esp, [eax + PCB_OFFSET_KERNEL_STACK]
    cmp dword [eax + PCB_OFFSET_FIRST_RUN], 0
    jnz .first_run
    ret
.first_run:
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iretd
