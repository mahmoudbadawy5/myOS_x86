#include <process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <string.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <fs/initrd.h>
#include <arch.h>

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t num_processes = 0;

pcb_t *current_process = NULL;

extern fs_node_t *root_dir;
extern void switch_to_process(pcb_t *next);

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
    pcb->state = PROCESS_STATE_READY;
    pcb->first_run = 1;
    
    fs_node_t *app_node = get_node((char *)app_path, root_dir);
    if (!app_node || !(app_node->flags & FS_FILE)) {
        printf("create_process: app not found: %s\n", app_path);
        num_processes--;
        return;
    }

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

    for (uint32_t i = 0; i < (app_node->size + BLOCK_SIZE - 1) / BLOCK_SIZE; i++) {
        void *virt = (void *)(USER_CODE_BASE + i * BLOCK_SIZE);
        void *phys = alloc_blocks(1);
        if (!phys) {
            set_page_dir(old_dir);
            num_processes--;
            return;
        }
        map_address_user(virt, phys);
    }
    seek_fs(app_node, 0, SEEK_START);
    read_fs(app_node, app_node->size, 1, (uint8_t *)USER_CODE_BASE);

    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        void *virt = (void *)(USER_STACK_TOP - (i + 1) * BLOCK_SIZE);
        void *phys = alloc_blocks(1);
        if (!phys) {
            set_page_dir(old_dir);
            num_processes--;
            return;
        }
        map_address_user(virt, phys);
    }

    uint32_t kstack_virt = KERNEL_STACK_BASE + process_index * KERNEL_STACK_SIZE;
    for (uint32_t i = 0; i < KERNEL_STACK_SIZE / BLOCK_SIZE; i++) {
        void *virt = (void *)(kstack_virt + i * BLOCK_SIZE);
        void *phys = alloc_blocks(1);
        if (!phys) {
            set_page_dir(old_dir);
            num_processes--;
            return;
        }
        map_address(virt, phys);
    }

    uint32_t iret_frame = kstack_virt + KERNEL_STACK_SIZE - 20;
    uint32_t *frame = (uint32_t *)iret_frame;
    frame[0] = USER_CODE_BASE;
    frame[1] = 0x1B;
    frame[2] = 0x202;
    frame[3] = USER_STACK_TOP - 4;
    frame[4] = 0x23;

    
    // printf("Mapped kernel stack\n");
    // printf("Calculation: KERNEL_STACK_BASE(%08ux) + process_index(%ud) * KERNEL_STACK_SIZE(%ud): %08ux\n", KERNEL_STACK_BASE, process_index, KERNEL_STACK_SIZE, KERNEL_STACK_BASE + process_index * KERNEL_STACK_SIZE);
    // printf("kstack_virt: %08ux\n", kstack_virt);
    // printf("KERNEL_STACK_SIZE: %d\n", KERNEL_STACK_SIZE);
    // printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
    // printf("KERNEL_STACK_SIZE / BLOCK_SIZE: %d\n", KERNEL_STACK_SIZE / BLOCK_SIZE);
    // printf("KERNEL_STACK_SIZE / BLOCK_SIZE * BLOCK_SIZE: %d\n", KERNEL_STACK_SIZE / BLOCK_SIZE * BLOCK_SIZE);
    // printf("KERNEL_STACK_SIZE - 20: %d\n", KERNEL_STACK_SIZE - 20);
    // printf("iret_frame: %08x\n", iret_frame);
    // printf("frame: %08x\n", frame);
    // printf("frame[0]: %08ux\n", frame[0]);
    // printf("frame[1]: %08ux\n", frame[1]);
    // printf("frame[2]: %08ux\n", frame[2]);
    // printf("frame[3]: %08ux\n", frame[3]);
    // printf("frame[4]: %08ux\n", frame[4]);
    pcb->kernel_stack_top = iret_frame;

    set_page_dir(old_dir);
}

void schedule(void)
{
    if (num_processes == 0)
        return;

    __asm__ __volatile__("cli");
    uint32_t next_index = 0;
    if (current_process) {
        current_process->state = PROCESS_STATE_READY;
        current_process->first_run = 0;
        for (uint32_t i = 0; i < num_processes; i++) {
            if (&process_table[i] == current_process) {
                next_index = (i + 1) % num_processes;
                break;
            }
        }
    }
    printf("We got to this point\n");

    pcb_t *next = &process_table[next_index];
    while (next->state == PROCESS_STATE_TERMINATED && num_processes > 1) {
        next_index = (next_index + 1) % num_processes;
        next = &process_table[next_index];
    }
    if (next->state == PROCESS_STATE_TERMINATED) {
        current_process = NULL;
        for (;;)
            __asm__ __volatile__("sti; hlt");
    }

    tss_set_stack(0x10, next->kernel_stack_top + 20);
    next->state = PROCESS_STATE_RUNNING;
    current_process = next;
    uint32_t *new_dir = (uint32_t*)next->regs.cr3;
    set_page_dir(new_dir); // Temporarily switch to the process's directory
    printf("Switching to process %d\n", next->pid);
    printf("next->kernel_stack_top: %08ux\n", next->kernel_stack_top);
    printf("next->cr3: %08ux\n", next->regs.cr3);
    uint32_t *check_frame = (uint32_t *)next->kernel_stack_top;
    printf("Stack Frame Check:\n");
    printf("EIP: %08ux, CS: %08ux, EFLAGS: %08ux\n", check_frame[0], check_frame[1], check_frame[2]);
    printf("ESP: %08ux, SS: %08ux\n", check_frame[3], check_frame[4]);
    switch_to_process(next);
}
