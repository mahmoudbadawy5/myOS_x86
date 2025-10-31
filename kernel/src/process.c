#include <process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <string.h>
#include <stdio.h>

static pcb_t process_table[MAX_PROCESSES];
static u32 next_pid = 1;
static u32 current_process_index = 0;

extern void switch_to_process(pcb_t* process);

void init_multitasking() {
    memset(process_table, 0, sizeof(process_table));
}

void create_process(void (*entry)()) {
    if (next_pid > MAX_PROCESSES) {
        printf("Max processes reached\n");
        return;
    }

    u32 process_index = next_pid - 1;
    pcb_t* pcb = &process_table[process_index];
    pcb->pid = next_pid++;
    pcb->state = PROCESS_STATE_READY;

    // Allocate a stack for the new process
    pcb->stack = (u8*)pmm_alloc_block();
    if (!pcb->stack) {
        printf("Failed to allocate stack for new process\n");
        return;
    }

    // Setup the initial register state
    memset(&pcb->regs, 0, sizeof(registers_t));
    pcb->regs.eip = (u32)entry;
    pcb->regs.esp = (u32)pcb->stack + PMM_BLOCK_SIZE;
    pcb->regs.eflags = 0x202; // Enable interrupts
    pcb->regs.cr3 = (u32)vmm_clone_directory();

    // Push initial register values onto the stack
    u32* stack_ptr = (u32*)pcb->regs.esp;
    *--stack_ptr = 0x202; // EFLAGS
    *--stack_ptr = 0x08;  // CS
    *--stack_ptr = (u32)entry; // EIP
    *--stack_ptr = 0;    // EAX
    *--stack_ptr = 0;    // EBX
    *--stack_ptr = 0;    // ECX
    *--stack_ptr = 0;    // EDX
    *--stack_ptr = 0;    // ESI
    *--stack_ptr = 0;    // EDI
    *--stack_ptr = 0;    // EBP
    *--stack_ptr = 0x10; // DS
    *--stack_ptr = 0x10; // ES
    *--stack_ptr = 0x10; // FS
    *--stack_ptr = 0x10; // GS
    pcb->regs.esp = (u32)stack_ptr;
}

void schedule() {
    if (next_pid <= 2) {
        return;
    }

    pcb_t* current_process = &process_table[current_process_index];
    if (current_process->state == PROCESS_STATE_RUNNING) {
        current_process->state = PROCESS_STATE_READY;
    }

    current_process_index = (current_process_index + 1) % (next_pid - 1);
    pcb_t* next_process = &process_table[current_process_index];
    next_process->state = PROCESS_STATE_RUNNING;

    switch_to_process(next_process);
}
