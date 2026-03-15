#include <process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/vmm.h>
#include <string.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <fs/initrd.h>
#include <arch.h>
#include <isr.h>

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t num_processes = 0, cur_proccess_id = -1;

pcb_t *current_process = NULL;

extern fs_node_t *root_dir;
extern void switch_to_process(pcb_t *next, struct regs* r);

void init_multitasking(void)
{
    memset((unsigned char *)process_table, 0, sizeof(process_table));
    current_process = NULL;
    next_pid = 1;
    num_processes = 0;
}

void create_process(const char *app_path)
{
    if (num_processes >= MAX_PROCESSES)
        return;

    uint32_t process_index = num_processes;
    pcb_t *pcb = &process_table[process_index];
    num_processes++;
    pcb->pid = next_pid++;
    pcb->state = PROCESS_STATE_NEW;
    
    fs_node_t *app_node = get_node((char *)app_path, root_dir);
    if (!app_node || !(app_node->flags & FS_FILE)) {
        printf("create_process: app not found: %s\n", app_path);
        num_processes--;
        return;
    }

    uint32_t kstack_virt = KERNEL_STACK_BASE + (num_processes * KERNEL_STACK_SIZE);
    for (int i=0; i < KERNEL_STACK_SIZE/4096; i++) {
        map_address((void*)(kstack_virt + i*4096), alloc_blocks(1));
    }

    uint32_t kstack_base = kstack_virt + KERNEL_STACK_SIZE;
    uint32_t iret_frame = (kstack_base - 32) & ~0xF;
    uint32_t *frame = (uint32_t *)iret_frame;
    frame[0] = USER_CODE_BASE;
    frame[1] = 0x1B;
    frame[2] = 0x202;
    frame[3] = USER_STACK_TOP - 16;
    frame[4] = 0x23;
    pcb->kernel_stack_top = iret_frame;

    uint32_t *old_dir = vmm_get_directory();
    uint32_t *new_dir = vmm_clone_directory();
    set_page_dir(new_dir);

    memset((unsigned char *)&pcb->regs, 0, sizeof(registers_t));
    pcb->regs.cr3 = (uint32_t)new_dir;
    pcb->regs.eflags = 0x202;
    pcb->regs.eip = USER_CODE_BASE;
    pcb->regs.esp = USER_STACK_TOP - 4;
    pcb->regs.cs = 0x1B;
    pcb->regs.ds = 0x23;
    pcb->regs.es = 0x23;
    pcb->regs.fs = 0x23;
    pcb->regs.gs = 0x23;
    pcb->regs.ss = 0x23;

    printf("Starting");

    alloc_mem_area(pcb, USER_CODE_BASE, app_node->size, VMA_READ|VMA_EXEC);
    printf("Allocated");
    seek_fs(app_node, 0, SEEK_START);
    read_fs(app_node, app_node->size, 1, (uint8_t *)pcb->memory_regions->start);
    printf("Loaded");

    
    alloc_mem_area(pcb, USER_STACK_TOP - USER_STACK_PAGES * BLOCK_SIZE, USER_STACK_PAGES * BLOCK_SIZE, VMA_READ|VMA_WRITE|VMA_STACK);
    set_page_dir(old_dir);
}

int cnt=0;

void schedule(struct regs *r)
{
    // TODO: make it a linked list so that no zombie processes remain
    if (num_processes == 0)
        return;

    __asm__ __volatile__("cli");
    if(cur_proccess_id != -1 && process_table[cur_proccess_id].state == PROCESS_STATE_RUNNING) {
        process_table[cur_proccess_id].state = PROCESS_STATE_READY;
    }
    int original_process_id = cur_proccess_id;
    do {
        cur_proccess_id = (cur_proccess_id + 1) % num_processes;
        if(process_table[cur_proccess_id].state == PROCESS_STATE_READY || process_table[cur_proccess_id].state == PROCESS_STATE_NEW) break;
    } while(cur_proccess_id != original_process_id);

    pcb_t *next = &process_table[cur_proccess_id];
    if (next->state == PROCESS_STATE_TERMINATED) {
        current_process = NULL;
        __asm__ __volatile__("sti; hlt");
    }
    
    // printf("Switching to process %d\n", cur_proccess_id);
    // printf("EIP = %08ux, ESP = %08ux\n", next->regs.eip, next->regs.esp);
    // cnt++;
    // if(cnt>=15) return;
    
    uint32_t kstack_top = KERNEL_STACK_BASE + ((cur_proccess_id + 1) * KERNEL_STACK_SIZE);
    tss_set_stack(0x10, kstack_top);
    
    switch_to_process(next, r);
       
    // Should never reach here
    for(;;);
}