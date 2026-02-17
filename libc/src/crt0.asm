[BITS 32]
global _start
extern main
section .text._start    ; Use this specific name

_start:
    ; 2. Call main
    call main

    ; 3. Exit System Call
    mov eax, 5          ; Assuming 1 is sys_exit
    int 0x80
    
    jmp $               ; Loop forever if exit fails