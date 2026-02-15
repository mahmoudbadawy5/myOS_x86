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
    ; Note: first_run is already 0 (that's why we're here), so no need to set it
    
.load_next:
    ; Update current_process pointer
    mov [current_process], eax
    
    ; Switch page directory
    mov ecx, [eax + PCB_OFFSET_CR3]
    mov cr3, ecx
    
    ; Check if this is the first run BEFORE loading the stack
    cmp dword [eax + PCB_OFFSET_FIRST_RUN], 0
    jnz .first_run
    
    ; NOT first run - restore interrupt frame and return to process
    ; Load saved kernel stack pointer
    mov esp, [eax + PCB_OFFSET_KERNEL_STACK]
    
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
    ; Load the IRET frame
    mov esp, [eax + PCB_OFFSET_KERNEL_STACK]
    
    ; Clear first_run flag so next time we'll save context properly
    mov dword [eax + PCB_OFFSET_FIRST_RUN], 0
    
    ; Stack now has IRET frame: [EIP, CS, EFLAGS, ESP, SS]
    ; CRITICAL: Set data segments to user mode BEFORE iretd
    ; The user code needs these to access data at CPL=3
    ; This is safe because:
    ; 1. Interrupts are disabled (CLI in schedule, IRET will re-enable)
    ; 2. We're about to iretd immediately, so no kernel code will run
    ; 3. Loading user segments at CPL=0 is legal
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; Now do the IRET - this will:
    ; - Pop EIP (user code), CS (0x1B = CPL 3), EFLAGS (IF=1)
    ; - Pop ESP (user stack), SS (0x23 = user data)
    ; - Switch to CPL=3 and jump to user code
    ; - Re-enable interrupts (IF flag in EFLAGS)
    iretd