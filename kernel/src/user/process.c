#include <process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <string.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <fs/initrd.h>

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t current_process_index = 0;

extern fs_node_t *root_dir;

extern void switch_to_process(pcb_t* process); // Implemented in process.asm

void init_multitasking() {
    memset(process_table, 0, sizeof(process_table));
}

void create_process(char* app_name) {
    if (next_pid > MAX_PROCESSES) {
        panic("Max processes reached\n");
        return;
    }

    uint32_t process_index = next_pid - 1;
    pcb_t* pcb = &process_table[process_index];
    pcb->pid = next_pid++;
    pcb->state = PROCESS_STATE_READY;

    // Setup the initial register state
    memset(&pcb->regs, 0, sizeof(registers_t));
    pcb->regs.eflags = 0x202; // Enable interrupts
    pcb->regs.cr3 = (uint32_t)vmm_clone_directory();

    uint32_t* old_page_dir = vmm_get_directory();
    set_page_dir((uint32_t*)pcb->regs.cr3);

    fs_node_t* app_node = finddir_fs(root_dir, app_name);
    if (!app_node) {
        panic("App not found");
    }

    // Allocate memory for the app
    for (uint32_t i = 0; i < app_node->size; i += BLOCK_SIZE) {
        void* virt_addr = (void*)(0x40000000 + i);
        void* phys_addr = alloc_blocks(1);
        map_address(virt_addr, phys_addr);
    }

    read_fs(app_node, 0, app_node->size, (uint8_t*)0x40000000);

    // Allocate stack
    for (int i = 0; i < 4; i++) {
        void* virt_addr = (void*)(0xE0000000 - (i+1)*BLOCK_SIZE);
        void* phys_addr = alloc_blocks(1);
        map_address(virt_addr, phys_addr);
    }

    set_page_dir(old_page_dir);

    pcb->regs.eip = 0x40000000;
    pcb->regs.esp = 0xE0000000;
    
    // User mode segments
    pcb->regs.cs = 0x1B;
    pcb->regs.ds = 0x23;
    pcb->regs.es = 0x23;
    pcb->regs.fs = 0x23;
    pcb->regs.gs = 0x23;
    pcb->regs.ss = 0x23;
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
