#include <process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <string.h>
#include <stdio.h>

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t current_process_index = 0;

extern void switch_to_process(pcb_t* process); // Implemented in process.asm

void init_multitasking() {
    memset(process_table, 0, sizeof(process_table));
}

void create_process(void (*entry)()) {
    if (next_pid > MAX_PROCESSES) {
        panic("Max processes reached\n");
        return;
    }

    uint32_t process_index = next_pid - 1;
    pcb_t* pcb = &process_table[process_index];
    pcb->pid = next_pid++;
    pcb->state = PROCESS_STATE_READY;

    // Allocate a stack for the new process
    pcb->stack = (uint8_t*)malloc(8192 * 2);
    if (!pcb->stack) {
        printf("Failed to allocate stack for new process\n");
        return;
    }

    // Setup the initial register state
    memset(&pcb->regs, 0, sizeof(registers_t));
    pcb->regs.eip = (uint32_t)entry;
    pcb->regs.esp = (uint32_t)pcb->stack + PMM_BLOCK_SIZE;
    pcb->regs.eflags = 0x202; // Enable interrupts
    pcb->regs.cr3 = (uint32_t)vmm_clone_directory();

    // Push initial register values onto the stack
    uint32_t* stack_ptr = (uint32_t*)pcb->regs.esp;
    *--stack_ptr = 0x202; // EFLAGS
    *--stack_ptr = 0x08;  // CS
    *--stack_ptr = (uint32_t)entry; // EIP
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
    pcb->regs.esp = (uint32_t)stack_ptr;
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
