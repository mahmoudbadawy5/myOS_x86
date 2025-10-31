[GLOBAL switch_to_process]

switch_to_process:
    ; Save registers
    pushad
    push ds
    push es
    push fs
    push gs

    mov eax, [esp + 44] ; Get pcb_t* argument

    ; Save current process state
    mov [eax + 28], esp ; Save ESP

    ; Load next process state
    mov esp, [eax + 28] ; Load ESP

    ; Restore registers
    pop gs
    pop fs
    pop es
    pop ds
    popad
    
    ; Return to new process
    iretd
