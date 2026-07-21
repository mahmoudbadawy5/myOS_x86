MAX_SYSCALL EQU 9

global handle_syscalls
extern syscalls

handle_syscalls:
    ; Already on stack (by CPU): user_SS, user_ESP, eflags, user_CS, user_EIP
    ; Build a stack frame identical to the one expected by `struct regs` in [isr.h]
    ; so that `schedule(regs)` + `switch_to_process` can restore correctly.

    push dword 0       ; dummy err_code
    push dword 0x80    ; dummy interrupt number

    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10       ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebx, esp       ; ebx = pointer to struct regs (top starts at gs)
    mov eax, [ebx + 44] ; regs->eax holds syscall number

    cmp eax, MAX_SYSCALL
    ja invalid_syscall

    push ebx
    call [syscalls + 4*eax]
    add esp, 4

    ; Write return value into regs->eax so user sees it in the saved EAX.
    mov [ebx + 44], eax

    jmp restore_syscalls

invalid_syscall:
    mov eax, -1
    mov [ebx + 44], eax

restore_syscalls:
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd