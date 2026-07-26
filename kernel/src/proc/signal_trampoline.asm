; Signal trampoline — mapped into every user process at SIGNAL_TRAMPOLINE_VADDR
; When a signal handler returns, it jumps here.  This stub calls sigreturn
; (syscall #28) to restore the original user context saved in the PCB.

section .text

global signal_trampoline_start
global signal_trampoline_end

signal_trampoline_start:
    mov eax, 28          ; SYS_sigreturn
    int 0x80             ; syscall into kernel
    ud2                  ; should never reach here
signal_trampoline_end:
