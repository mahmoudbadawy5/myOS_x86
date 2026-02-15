MAX_SYSCALL EQU 6

global handle_syscalls
extern syscalls

handle_syscalls:
    ; Already on stack (by CPU): user_SS, user_ESP, eflags, user_CS, user_EIP
    ; Push regs in order of syscall_regs_t: esp placeholder, ebx, ecx, edx, esi, edi, ebp, ds, es, fs, gs, eax

    cmp EAX, MAX_SYSCALL
    ja invalid_syscall

    push EAX
    push GS
    push FS
    push ES
    push DS
    push EBP
    push EDI
    push ESI
    push EDX
    push ECX
    push EBX
    push ESP
    ; Now ESP points to saved-ESP slot; syscall_regs_t layout starts here

    push ESP
    mov EAX, [ESP+4+44]
    call [syscalls + 4*EAX]
    mov [ESP+4+44], EAX
    add ESP, 4
    pop EBX
    pop ECX
    pop EDX
    pop ESI
    pop EDI
    pop EBP
    pop DS
    pop ES
    pop FS
    pop GS
    add ESP, 4
    iretd

invalid_syscall:
    mov EAX, -1
    iret