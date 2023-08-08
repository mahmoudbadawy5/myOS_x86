print("; Making isr handling functions accessable")

for i in range(32):
    print(f"global isr{i}")


print("; Adding implementations")
for i in range(32):
    """
    With error codes: 8, [10-14]
    """
    if i == 8 or (i >= 10 and i <= 14):
        print(f"""isr{i}:
    cli ; No need for pushing a dummy 0 here
    push byte {i}
    jmp isr_common_stub
""")
    else:
        print(f"""isr{i}:
    cli
    push byte 0    ; A normal ISR stub that pops a dummy error code to keep a
                   ; uniform stack frame
    push byte {i}
    jmp isr_common_stub
""")

print("""
; We call a C function in here. We need to let the assembler know
; that 'fault_handler' exists in another file
extern fault_handler

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10   ; Load the Kernel Data Segment descriptor!
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, esp   ; Push us the stack
    push eax
    mov eax, fault_handler
    call eax       ; A special call, preserves the 'eip' register
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8     ; Cleans up the pushed error code and pushed ISR number
    iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!
		
""")
