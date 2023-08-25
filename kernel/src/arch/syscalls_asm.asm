MAX_SYSCALL EQU 4

global handle_syscalls
extern syscalls

handle_syscalls:
    ; Already on stack: SS, SP, FLAGS, CS, IP
    ; Need to push: EAX, GS, FS, ES, DS, EBP, EDI, ESI, EDX, ECX, EBX

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

    call [syscalls + 4*EAX]

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