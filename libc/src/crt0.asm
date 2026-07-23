[BITS 32]
global _start
extern main
section .text._start    ; Use this specific name

_start:
    ; Stack layout: [argc] [argv[0]] [argv[1]] ... [argv[n]] [NULL] [NULL]
    pop eax             ; eax = argc
    mov ebx, esp        ; ebx = pointer to argv[0]

    ; Push envp = NULL (simplified, no envp support yet)
    push dword 0

    ; Push argv
    push ebx

    ; Push argc
    push eax

    call main

    ; Exit System Call
    mov eax, 5
    int 0x80
    
    jmp $               ; Loop forever if exit fails