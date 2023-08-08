for i in range(16):
    print(f"global irq{i}")

for i in range(16):
    print(f"""; {i+32}: IRQ{i}
irq{i}:
    cli
    push byte 0    ; Note that these don't push an error code on the stack:
                   ; We need to push a dummy error code
    push byte {i+32}
    jmp irq_common_stub
""")


print("""
extern irq_handler

; This is a stub that we have created for IRQ based ISRs. This calls
; 'irq_handler' in our C code. We need to create this in an 'irq.c'
irq_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, esp
    push eax
    mov eax, irq_handler
    call eax
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret
""")
