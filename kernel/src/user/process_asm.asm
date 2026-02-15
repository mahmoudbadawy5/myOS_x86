[GLOBAL switch_to_process]
[EXTERN current_process]

PCB_OFFSET_FIRST_RUN     equ 8
PCB_OFFSET_KERNEL_STACK  equ 12
PCB_OFFSET_CR3           equ 56

; Context switch function - NEVER RETURNS!
; Called from timer interrupt, always switches context via IRET
switch_to_process:
    mov eax, [esp + 4]              ; Get next process pointer
    mov ecx, [current_process]      ; Get current process pointer
    
    ; Save current process context if it exists
    test ecx, ecx
    jz .load_next                   ; No current process, just load next
    
    ; Check if current process is running for first time
    cmp dword [ecx + PCB_OFFSET_FIRST_RUN], 0
    jnz .load_next                  ; First run, nothing to save
    
    ; Save current ESP (which points to interrupt frame on kernel stack)
    mov [ecx + PCB_OFFSET_KERNEL_STACK], esp
    
.load_next:
    ; Update current_process pointer
    mov [current_process], eax
    
    ; Switch page directory
    mov ecx, [eax + PCB_OFFSET_CR3]
    mov cr3, ecx
    
    ; Load new process's kernel stack
    mov esp, [eax + PCB_OFFSET_KERNEL_STACK]
    
    ; Check if this is the first run of the new process
    cmp dword [eax + PCB_OFFSET_FIRST_RUN], 0
    jnz .first_run
    
    ; NOT first run - restore interrupt frame and return to process
    ; Stack currently has the saved interrupt frame:
    ; [esp+0]  = GS
    ; [esp+4]  = FS  
    ; [esp+8]  = ES
    ; [esp+12] = DS
    ; [esp+16] = EDI
    ; [esp+20] = ESI
    ; [esp+24] = EBP
    ; [esp+28] = (ignored ESP)
    ; [esp+32] = EBX
    ; [esp+36] = EDX
    ; [esp+40] = ECX
    ; [esp+44] = EAX
    ; [esp+48] = Error Code (dummy 0)
    ; [esp+52] = Interrupt Number
    ; [esp+56] = EIP (pushed by CPU)
    ; [esp+60] = CS  (pushed by CPU)
    ; [esp+64] = EFLAGS (pushed by CPU)
    ; [esp+68] = ESP (pushed by CPU, user mode)
    ; [esp+72] = SS  (pushed by CPU, user mode)
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8      ; Skip error code and interrupt number
    iret            ; Return to process (pops EIP, CS, EFLAGS, ESP, SS)
    
.first_run:
    ; First run - jump directly to user mode
    ; Stack has IRET frame: [EIP, CS, EFLAGS, ESP, SS]
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iretd