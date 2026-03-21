#include <types.h>
#include <stdio.h>
#include <string.h>
#include <proc/elf.h>
#include <proc/process.h>
#include <fs/vfs.h>
#include <mem/malloc.h>
#include <mem/vmm.h>
#include <mem/phys_mem.h>


bool elf_check_file(Elf32_Ehdr *hdr) {
	if(!hdr) return false;
	if(hdr->e_ident[EI_MAG0] != ELFMAG0) {
		ERROR("ELF Header EI_MAG0 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG1] != ELFMAG1) {
		ERROR("ELF Header EI_MAG1 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG2] != ELFMAG2) {
		ERROR("ELF Header EI_MAG2 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG3] != ELFMAG3) {
		ERROR("ELF Header EI_MAG3 incorrect.\n");
		return false;
	}
	return true;
}


bool elf_check_supported(Elf32_Ehdr *hdr) {
	if(!elf_check_file(hdr)) {
		ERROR("Invalid ELF File.\n");
		return false;
	}
	if(hdr->e_ident[EI_CLASS] != ELFCLASS32) {
		ERROR("Unsupported ELF File Class.\n");
		return false;
	}
	if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
		ERROR("Unsupported ELF File byte order.\n");
		return false;
	}
	if(hdr->e_machine != EM_386) {
		ERROR("Unsupported ELF File target.\n");
		return false;
	}
	if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
		ERROR("Unsupported ELF File version.\n");
		return false;
	}
	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {
		ERROR("Unsupported ELF File type.\n");
		return false;
	}
	return true;
}


int load_elf(pcb_t* proc, const char* path) {
	fs_node_t *app_node = get_node((char *)path, root_dir);
    if (!app_node || !(app_node->flags & FS_FILE)) {
        ERROR("load_elf: app not found: %s\n", path);
        return -1;
	}
	Elf32_Ehdr *hdr = malloc(sizeof(Elf32_Ehdr));
	seek_fs(app_node, 0, SEEK_START);
    read_fs(app_node, sizeof(Elf32_Ehdr), 1, (uint8_t*)hdr);
	if(!elf_check_file(hdr)) return -2;
	if(!elf_check_supported(hdr)) return -3;

	uint32_t max_proc_end = 0;

	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf32_Phdr* phdr = malloc(sizeof(Elf32_Phdr));
		seek_fs(app_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
    	read_fs(app_node, sizeof(Elf32_Phdr), 1, (uint8_t*)phdr);
		if(phdr->p_type == PT_LOAD) {
			uint32_t flags = VMA_USER;
            if (phdr->p_flags & 0x1) flags |= VMA_EXEC;
            if (phdr->p_flags & 0x2) flags |= VMA_WRITE;
            if (phdr->p_flags & 0x4) flags |= VMA_READ;
			alloc_mem_area(proc, phdr->p_vaddr, phdr->p_memsz, flags);
			uint32_t cur_end = phdr->p_vaddr + phdr->p_memsz;
			max_proc_end = max_proc_end < cur_end ? cur_end : max_proc_end;

			seek_fs(app_node, phdr->p_offset, SEEK_START);
            read_fs(app_node, phdr->p_filesz, 1, (uint8_t*)phdr->p_vaddr);

			if (phdr->p_memsz > phdr->p_filesz) {
                memset((void*)(phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            }
		}
	}

	uint32_t stack_size = USER_STACK_PAGES * BLOCK_SIZE;
	uint32_t stack_bottom = USER_STACK_TOP - stack_size;
	alloc_mem_area(proc, stack_bottom, stack_size, VMA_READ | VMA_WRITE | VMA_STACK | VMA_USER);

	max_proc_end = ALIGN_PAGE_UPWARDS(max_proc_end);
	alloc_mem_area(proc, max_proc_end, BLOCK_SIZE, VMA_READ | VMA_WRITE | VMA_HEAP | VMA_USER);

	proc->regs.eip = hdr->e_entry;
	proc->regs.esp = USER_STACK_TOP - 16;
	proc->state = PROCESS_STATE_NEW;

	uint32_t *frame = (uint32_t *)proc->kernel_stack_top;
	frame[0] = hdr->e_entry;
	frame[1] = 0x1B;
	frame[2] = 0x202;
	frame[3] = USER_STACK_TOP - 16;
	frame[4] = 0x23;
	return 0;
}