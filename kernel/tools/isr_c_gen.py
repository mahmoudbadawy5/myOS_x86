print("#include <isr.h>")
print("#include <idt.h>")
print("#include <stdio.h>")
print("#include <mem/virt_mem.h>")
print("""
/* These are function prototypes for all of the exception
*  handlers: The first 32 entries in the IDT are reserved
*  by Intel, and are designed to service exceptions! */

#ifndef _ISR_HH
#define _ISR_HH      
""")

for i in range(32):
    print(f"extern void isr{i}();")

print("""
#endif
/* This is a very repetitive function... it's not hard, it's
*  just annoying. As you can see, we set the first 32 entries
*  in the IDT to the first 32 ISRs. We can't use a for loop
*  for this, because there is no way to get the function names
*  that correspond to that given entry. We set the access
*  flags to 0x8E. This means that the entry is present, is
*  running in ring 0 (kernel level), and has the lower 5 bits
*  set to the required '14', which is represented by 'E' in
*  hex. */
void isrs_install()
{
""")

for i in range(32):
    print(f"\tidt_set_gate({i}, (unsigned)isr{i}, 0x08, 0x8E);")
print("}")

print("""
/* This is a simple string array. It contains the message that
*  corresponds to each and every exception. We get the correct
*  message by accessing like:
*  exception_message[interrupt_number] */
char *exception_messages[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};
""")


print("""
/* All of our Exception handling Interrupt Service Routines will
*  point to this function. This will tell us what exception has
*  happened! Right now, we simply halt the system by hitting an
*  endless loop. All ISRs disable interrupts while they are being
*  serviced as a 'locking' mechanism to prevent an IRQ from
*  happening and messing up kernel data structures */
void fault_handler(struct regs *r)
{
    switch_to_kernel_page_dir();
    if (r->int_no < 32)
    {
        unsigned int cpl = r->cs & 3;
        puts("\\n--- Exception dump ---");
        printf("Exception: %s (int_no=%d)\\n", exception_messages[r->int_no], r->int_no);
        printf("EIP=0x%08x  CS=0x%04x (CPL=%d)  SS=0x%04x\\n", r->eip, r->cs & 0xFFFF, cpl, r->ss & 0xFFFF);
        printf("ESP=0x%08x  EFLAGS=0x%08x\\n", r->useresp, r->eflags);
        printf("err_code=0x%08x\\n", r->err_code);
        printf("EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x\\n", r->eax, r->ebx, r->ecx, r->edx);
        printf("ESI=0x%08x EDI=0x%08x EBP=0x%08x\\n", r->esi, r->edi, r->ebp);
        puts("-----------------------\\n");
        panic("%s Exception. System Halted!", exception_messages[r->int_no]);
        for (;;);
    }
}""")
