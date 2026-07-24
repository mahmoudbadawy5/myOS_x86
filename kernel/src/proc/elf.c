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
	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
		ERROR("Unsupported ELF File type.\n");
		return false;
	}
	return true;
}


/*
 * load_elf — Load an ELF executable (ET_EXEC) or PIE executable (ET_DYN)
 * into the given process.
 */
int load_elf(pcb_t* proc, const char* path) {
	fs_node_t *app_node = get_node((char *)path, root_dir);
    if (!app_node || !(app_node->flags & FS_FILE)) {
        ERROR("load_elf: app not found: %s\n", path);
        return -1;
	}
	Elf32_Ehdr *hdr = malloc(sizeof(Elf32_Ehdr));
	seek_fs(app_node, 0, SEEK_START);
    read_fs(app_node, sizeof(Elf32_Ehdr), 1, (uint8_t*)hdr);
	if(!elf_check_file(hdr)) { free(hdr); return -2; }
	if(!elf_check_supported(hdr)) { free(hdr); return -3; }

	uint32_t max_proc_end = 0;
	uint32_t first_load_vaddr = 0;

	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf32_Phdr* phdr = malloc(sizeof(Elf32_Phdr));
		seek_fs(app_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
    	read_fs(app_node, sizeof(Elf32_Phdr), 1, (uint8_t*)phdr);
		if(phdr->p_type == PT_LOAD) {
			uint32_t flags = VMA_USER;
            if (phdr->p_flags & 0x1) flags |= VMA_EXEC;
            if (phdr->p_flags & 0x2) flags |= VMA_WRITE;
            if (phdr->p_flags & 0x4) flags |= VMA_READ;

			/* For ET_DYN (PIE), compute the relocation offset */
			uint32_t vaddr = phdr->p_vaddr;
			if (hdr->e_type == ET_DYN) {
				if (first_load_vaddr == 0)
					first_load_vaddr = phdr->p_vaddr;
			}

			alloc_mem_area(proc, vaddr, phdr->p_memsz, flags);
			uint32_t cur_end = vaddr + phdr->p_memsz;
			max_proc_end = max_proc_end < cur_end ? cur_end : max_proc_end;

			seek_fs(app_node, phdr->p_offset, SEEK_START);
            read_fs(app_node, phdr->p_filesz, 1, (uint8_t*)vaddr);

			if (phdr->p_memsz > phdr->p_filesz) {
                memset((void*)(vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            }
		}
		free(phdr);
	}

	/* For ET_DYN executables, parse dynamic section and load shared libs */
	if (hdr->e_type == ET_DYN) {
		/* Find PT_DYNAMIC */
		for (int i = 0; i < hdr->e_phnum; i++) {
			Elf32_Phdr *phdr = malloc(sizeof(Elf32_Phdr));
			seek_fs(app_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
			read_fs(app_node, sizeof(Elf32_Phdr), 1, (uint8_t *)phdr);

			if (phdr->p_type == PT_DYNAMIC) {
				uint32_t dyn_size = phdr->p_filesz;
				Elf32_Dyn *dyns = malloc(dyn_size);
				seek_fs(app_node, phdr->p_offset, SEEK_START);
				read_fs(app_node, dyn_size, 1, (uint8_t *)dyns);

				/* Extract dynamic tags */
				Elf32_Rel *reltab = NULL;
				uint32_t relsz = 0;
				Elf32_Rel *jmprel = NULL;
				uint32_t jmprelsz = 0;
				Elf32_Sym *dyn_symtab = NULL;
				const char *dyn_strtab = NULL;
				Elf32_Dyn *needed[16];
				int needed_count = 0;

				for (uint32_t j = 0; dyns[j].d_tag != DT_NULL; j++) {
					switch (dyns[j].d_tag) {
						case DT_STRTAB:
							dyn_strtab = (const char *)dyns[j].d_un.d_ptr;
							break;
						case DT_SYMTAB:
							dyn_symtab = (Elf32_Sym *)dyns[j].d_un.d_ptr;
							break;
						case DT_REL:
							reltab = (Elf32_Rel *)dyns[j].d_un.d_ptr;
							break;
						case DT_RELSZ:
							relsz = dyns[j].d_un.d_val;
							break;
						case DT_JMPREL:
							jmprel = (Elf32_Rel *)dyns[j].d_un.d_ptr;
							break;
						case DT_PLTRELSZ:
							jmprelsz = dyns[j].d_un.d_val;
							break;
						case DT_NEEDED:
							if (needed_count < 16)
								needed[needed_count++] = &dyns[j];
							break;
					}
				}

				/* Load each DT_NEEDED shared library */
				/* Build a simple symbol table: name -> address pairs */
				typedef struct { const char *name; uint32_t addr; } sym_entry_t;
				sym_entry_t global_syms[256];
				int global_sym_count = 0;

				uint32_t next_lib_base = SHLIB_BASE;

				for (int n = 0; n < needed_count; n++) {
					const char *lib_name = dyn_strtab + needed[n]->d_un.d_val;
					char lib_path[256];
					/* Build path: look in /lib/ */
					lib_path[0] = '/';
					lib_path[1] = 'l';
					lib_path[2] = 'i';
					lib_path[3] = 'b';
					lib_path[4] = '/';
					int pi = 5;
					const char *s = lib_name;
					while (*s && pi < 254) lib_path[pi++] = *s++;
					lib_path[pi] = '\0';

					uint32_t lib_base = next_lib_base;
					uint32_t lib_top = load_shared_library(proc, lib_path, lib_base);
					if (lib_top == 0) {
						ERROR("load_elf: failed to load shared lib: %s\n", lib_name);
						continue;
					}
					next_lib_base = (lib_top + 0xFFF) & ~0xFFF; /* Align to page boundary */

					/* Read the library's dynamic section to get exported symbols */
					fs_node_t *lib_node = get_node((char *)lib_path, root_dir);
					if (!lib_node) continue;
					Elf32_Ehdr *lib_hdr = malloc(sizeof(Elf32_Ehdr));
					seek_fs(lib_node, 0, SEEK_START);
					read_fs(lib_node, sizeof(Elf32_Ehdr), 1, (uint8_t *)lib_hdr);

					for (int i = 0; i < lib_hdr->e_phnum; i++) {
						Elf32_Phdr *lphdr = malloc(sizeof(Elf32_Phdr));
						seek_fs(lib_node, lib_hdr->e_phoff + (i * lib_hdr->e_phentsize), SEEK_START);
						read_fs(lib_node, sizeof(Elf32_Phdr), 1, (uint8_t *)lphdr);

						if (lphdr->p_type == PT_DYNAMIC) {
							Elf32_Dyn *ldyns = malloc(lphdr->p_filesz);
							seek_fs(lib_node, lphdr->p_offset, SEEK_START);
							read_fs(lib_node, lphdr->p_filesz, 1, (uint8_t *)ldyns);

							const char *lstrtab = NULL;
							Elf32_Sym *lsymtab = NULL;
							uint32_t lhash_off = 0;

							for (uint32_t k = 0; ldyns[k].d_tag != DT_NULL; k++) {
								switch (ldyns[k].d_tag) {
									case DT_STRTAB:
										lstrtab = (const char *)(ldyns[k].d_un.d_ptr + lib_base);
										break;
									case DT_SYMTAB:
										lsymtab = (Elf32_Sym *)(ldyns[k].d_un.d_ptr + lib_base);
										break;
									case DT_HASH:
										lhash_off = ldyns[k].d_un.d_val;
										break;
								}
							}

							/* Read nchain from hash table to get symbol count */
							uint32_t sym_count = 0;
							if (lhash_off) {
								uint32_t *hash = malloc(8);
								seek_fs(lib_node, lhash_off, SEEK_START);
								read_fs(lib_node, 8, 1, (uint8_t *)hash);
								sym_count = hash[1];
								free(hash);
							}

							/* Enumerate all exported symbols from the library */
							if (lsymtab && lstrtab && sym_count > 0) {
								for (uint32_t k = 1; k < sym_count; k++) {
									if (lsymtab[k].st_shndx == SHN_UNDEF) continue;
									if (ELF32_ST_BIND(lsymtab[k].st_info) != STB_GLOBAL &&
									    ELF32_ST_BIND(lsymtab[k].st_info) != STB_WEAK) continue;
									const char *sym_name = lstrtab + lsymtab[k].st_name;
									if (global_sym_count < 256) {
										global_syms[global_sym_count].name = sym_name;
										global_syms[global_sym_count].addr = lsymtab[k].st_value + lib_base;
										global_sym_count++;
									}
								}
							}

							free(ldyns);
							break;
						}
						free(lphdr);
					}
					free(lib_hdr);
				}

				/* If no DT_NEEDED found, default-load libc.so */
				if (needed_count == 0) {
					uint32_t lib_base = load_shared_library(proc, "/lib/libc.so", SHLIB_BASE);
					if (lib_base != 0) {
						fs_node_t *lib_node = get_node("/lib/libc.so", root_dir);
						if (lib_node) {
							Elf32_Ehdr *lib_hdr = malloc(sizeof(Elf32_Ehdr));
							seek_fs(lib_node, 0, SEEK_START);
							read_fs(lib_node, sizeof(Elf32_Ehdr), 1, (uint8_t *)lib_hdr);

							for (int i = 0; i < lib_hdr->e_phnum; i++) {
								Elf32_Phdr *lphdr = malloc(sizeof(Elf32_Phdr));
								seek_fs(lib_node, lib_hdr->e_phoff + (i * lib_hdr->e_phentsize), SEEK_START);
								read_fs(lib_node, sizeof(Elf32_Phdr), 1, (uint8_t *)lphdr);

								if (lphdr->p_type == PT_DYNAMIC) {
									Elf32_Dyn *ldyns = malloc(lphdr->p_filesz);
									seek_fs(lib_node, lphdr->p_offset, SEEK_START);
									read_fs(lib_node, lphdr->p_filesz, 1, (uint8_t *)ldyns);

									const char *lstrtab = NULL;
									Elf32_Sym *lsymtab = NULL;
									uint32_t lhash_off = 0;

									for (uint32_t k = 0; ldyns[k].d_tag != DT_NULL; k++) {
										switch (ldyns[k].d_tag) {
											case DT_STRTAB:
												lstrtab = (const char *)(ldyns[k].d_un.d_ptr + lib_base);
												break;
											case DT_SYMTAB:
												lsymtab = (Elf32_Sym *)(ldyns[k].d_un.d_ptr + lib_base);
												break;
											case DT_HASH:
												lhash_off = ldyns[k].d_un.d_val;
												break;
										}
									}

									uint32_t sym_count = 0;
									if (lhash_off) {
										uint32_t *hash = malloc(8);
										seek_fs(lib_node, lhash_off, SEEK_START);
										read_fs(lib_node, 8, 1, (uint8_t *)hash);
										sym_count = hash[1];
										free(hash);
									}

									if (lsymtab && lstrtab && sym_count > 0) {
										for (uint32_t k = 1; k < sym_count; k++) {
											if (lsymtab[k].st_shndx == SHN_UNDEF) continue;
											if (ELF32_ST_BIND(lsymtab[k].st_info) != STB_GLOBAL &&
											    ELF32_ST_BIND(lsymtab[k].st_info) != STB_WEAK) continue;
											const char *sym_name = lstrtab + lsymtab[k].st_name;
											if (global_sym_count < 256) {
												global_syms[global_sym_count].name = sym_name;
												global_syms[global_sym_count].addr = lsymtab[k].st_value + lib_base;
												global_sym_count++;
											}
										}
									}

									free(ldyns);
									break;
								}
								free(lphdr);
							}
							free(lib_hdr);
						}
					}
				}

				/* Resolve relocations in the app using the global symbol table */
				uint32_t app_reloc_offset = 0; /* 0 for ET_DYN loaded at preferred address */

				/* Process REL relocations (.rel.dyn) */
				if (reltab && relsz) {
					uint32_t count = relsz / sizeof(Elf32_Rel);
					for (uint32_t j = 0; j < count; j++) {
						uint32_t type = ELF32_R_TYPE(reltab[j].r_info);
						uint32_t sym_idx = ELF32_R_SYM(reltab[j].r_info);
						uint32_t *loc = (uint32_t *)reltab[j].r_offset;

						switch (type) {
							case R_386_RELATIVE:
								*loc += app_reloc_offset;
								break;
							case R_386_32: {
								/* S + A */
								if (dyn_symtab && dyn_strtab && sym_idx != 0) {
									const char *sym_name = dyn_strtab + dyn_symtab[sym_idx].st_name;
									uint32_t sym_val = dyn_symtab[sym_idx].st_value + app_reloc_offset;
									/* Look up in global symbol table */
									for (int k = 0; k < global_sym_count; k++) {
										if (strcmp(global_syms[k].name, sym_name) == 0) {
											sym_val = global_syms[k].addr;
											break;
										}
									}
									*loc = sym_val;
								}
								break;
							}
							case R_386_PC32: {
								/* S + A - P */
								if (dyn_symtab && dyn_strtab && sym_idx != 0) {
									const char *sym_name = dyn_strtab + dyn_symtab[sym_idx].st_name;
									uint32_t sym_val = dyn_symtab[sym_idx].st_value + app_reloc_offset;
									for (int k = 0; k < global_sym_count; k++) {
										if (strcmp(global_syms[k].name, sym_name) == 0) {
											sym_val = global_syms[k].addr;
											break;
										}
									}
									*loc = sym_val + *loc - (uint32_t)loc;
								}
								break;
							}
							case R_386_GLOB_DAT:
							case R_386_JMP_SLOT: {
								/* Resolve symbol address */
								if (dyn_symtab && dyn_strtab && sym_idx != 0) {
									const char *sym_name = dyn_strtab + dyn_symtab[sym_idx].st_name;
									uint32_t sym_val = 0;
									for (int k = 0; k < global_sym_count; k++) {
										if (strcmp(global_syms[k].name, sym_name) == 0) {
											sym_val = global_syms[k].addr;
											break;
										}
									}
									if (sym_val != 0) {
										*loc = sym_val;
									} else {
										ERROR("load_elf: unresolved symbol: %s\n", sym_name);
									}
								}
								break;
							}
						}
					}
				}

				/* Process JMPREL relocations (PLT entries) */
				if (jmprel && jmprelsz) {
					uint32_t count = jmprelsz / sizeof(Elf32_Rel);
					for (uint32_t j = 0; j < count; j++) {
						uint32_t type = ELF32_R_TYPE(jmprel[j].r_info);
						uint32_t sym_idx = ELF32_R_SYM(jmprel[j].r_info);
						uint32_t *loc = (uint32_t *)jmprel[j].r_offset;

						if (type == R_386_JMP_SLOT && dyn_symtab && dyn_strtab && sym_idx != 0) {
							const char *sym_name = dyn_strtab + dyn_symtab[sym_idx].st_name;
							uint32_t sym_val = 0;
							for (int k = 0; k < global_sym_count; k++) {
								if (strcmp(global_syms[k].name, sym_name) == 0) {
									sym_val = global_syms[k].addr;
									break;
								}
							}
							if (sym_val != 0) {
								*loc = sym_val;
							} else {
								ERROR("load_elf: unresolved PLT symbol: %s\n", sym_name);
							}
						}
					}
				}

				free(dyns);
				break;
			}
			free(phdr);
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
	free(hdr);
	return 0;
}


/*
 * load_shared_library — Load an ET_DYN shared object at load_addr.
 * Returns the top address (load_addr + image size) where it was loaded, or 0 on error.
 */
uint32_t load_shared_library(pcb_t *proc, const char *path, uint32_t load_addr)
{
	fs_node_t *lib_node = get_node((char *)path, root_dir);
	if (!lib_node || !(lib_node->flags & FS_FILE)) {
		return 0;
	}

	Elf32_Ehdr *hdr = malloc(sizeof(Elf32_Ehdr));
	seek_fs(lib_node, 0, SEEK_START);
	read_fs(lib_node, sizeof(Elf32_Ehdr), 1, (uint8_t *)hdr);

	if (!elf_check_file(hdr) || hdr->e_type != ET_DYN) {
		free(hdr);
		return 0;
	}

	/* Find first PT_LOAD to determine the offset within the file */
	uint32_t first_vaddr = 0;
	uint32_t max_end = 0;
	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf32_Phdr *phdr = malloc(sizeof(Elf32_Phdr));
		seek_fs(lib_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
		read_fs(lib_node, sizeof(Elf32_Phdr), 1, (uint8_t *)phdr);
		if (phdr->p_type == PT_LOAD) {
			if (first_vaddr == 0)
				first_vaddr = phdr->p_vaddr;
			uint32_t end = phdr->p_vaddr + phdr->p_memsz;
			if (end > max_end) max_end = end;
		}
		free(phdr);
	}

	uint32_t reloc_offset = load_addr - first_vaddr;

	/* Load all PT_LOAD segments */
	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf32_Phdr *phdr = malloc(sizeof(Elf32_Phdr));
		seek_fs(lib_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
		read_fs(lib_node, sizeof(Elf32_Phdr), 1, (uint8_t *)phdr);

		if (phdr->p_type == PT_LOAD) {
			uint32_t vaddr = phdr->p_vaddr + reloc_offset;
			uint32_t flags = VMA_USER;
			if (phdr->p_flags & 0x1) flags |= VMA_EXEC;
			if (phdr->p_flags & 0x2) flags |= VMA_WRITE;
			if (phdr->p_flags & 0x4) flags |= VMA_READ;

			alloc_mem_area(proc, vaddr, phdr->p_memsz, flags);

			seek_fs(lib_node, phdr->p_offset, SEEK_START);
			read_fs(lib_node, phdr->p_filesz, 1, (uint8_t *)vaddr);

			if (phdr->p_memsz > phdr->p_filesz) {
				memset((void *)(vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
			}
		}
		free(phdr);
	}

	/* Self-relocate: resolve the library's own PLT/GOT entries.
	 * The library calls its own functions through PLT stubs, so its
	 * GOT entries need to point to the actual functions within itself. */
	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf32_Phdr *phdr = malloc(sizeof(Elf32_Phdr));
		seek_fs(lib_node, hdr->e_phoff + (i * hdr->e_phentsize), SEEK_START);
		read_fs(lib_node, sizeof(Elf32_Phdr), 1, (uint8_t *)phdr);

		if (phdr->p_type == PT_DYNAMIC) {
			Elf32_Dyn *ldyns = malloc(phdr->p_filesz);
			seek_fs(lib_node, phdr->p_offset, SEEK_START);
			read_fs(lib_node, phdr->p_filesz, 1, (uint8_t *)ldyns);

			const char *lstrtab = NULL;
			Elf32_Sym *lsymtab = NULL;
			Elf32_Rel *ljmprel = NULL;
			uint32_t ljmprelsz = 0;
			uint32_t lhash_off = 0;

			Elf32_Rel *lreltab = NULL;
			uint32_t lrelsz = 0;

			for (uint32_t k = 0; ldyns[k].d_tag != DT_NULL; k++) {
				switch (ldyns[k].d_tag) {
					case DT_STRTAB:
						lstrtab = (const char *)(ldyns[k].d_un.d_ptr + load_addr);
						break;
					case DT_SYMTAB:
						lsymtab = (Elf32_Sym *)(ldyns[k].d_un.d_ptr + load_addr);
						break;
					case DT_JMPREL:
						ljmprel = (Elf32_Rel *)(ldyns[k].d_un.d_val + load_addr);
						break;
					case DT_PLTRELSZ:
						ljmprelsz = ldyns[k].d_un.d_val;
						break;
					case DT_REL:
						lreltab = (Elf32_Rel *)(ldyns[k].d_un.d_val + load_addr);
						break;
					case DT_RELSZ:
						lrelsz = ldyns[k].d_un.d_val;
						break;
					case DT_HASH:
						lhash_off = ldyns[k].d_un.d_val;
						break;
				}
			}

			uint32_t sym_count = 0;
			if (lhash_off) {
				uint32_t *hash = malloc(8);
				seek_fs(lib_node, lhash_off, SEEK_START);
				read_fs(lib_node, 8, 1, (uint8_t *)hash);
				sym_count = hash[1];
				free(hash);
			}

			if (ljmprel && ljmprelsz && lsymtab && lstrtab && sym_count > 0) {
				uint32_t count = ljmprelsz / sizeof(Elf32_Rel);
				for (uint32_t j = 0; j < count; j++) {
					uint32_t type = ELF32_R_TYPE(ljmprel[j].r_info);
					uint32_t sym_idx = ELF32_R_SYM(ljmprel[j].r_info);
					uint32_t *loc = (uint32_t *)(ljmprel[j].r_offset + load_addr);

					if (type == R_386_JMP_SLOT && sym_idx != 0 && sym_idx < sym_count) {
						uint32_t sym_val = lsymtab[sym_idx].st_value + load_addr;
						*loc = sym_val;
					}
				}
			}

			/* Process .rel.dyn (R_386_RELATIVE, R_386_GLOB_DAT) */
			if (lreltab && lrelsz) {
				uint32_t count = lrelsz / sizeof(Elf32_Rel);
				for (uint32_t j = 0; j < count; j++) {
					uint32_t type = ELF32_R_TYPE(lreltab[j].r_info);
					uint32_t sym_idx = ELF32_R_SYM(lreltab[j].r_info);
					uint32_t *loc = (uint32_t *)(lreltab[j].r_offset + load_addr);

					if (type == R_386_RELATIVE) {
						*loc += load_addr;
					} else if ((type == R_386_GLOB_DAT || type == R_386_JMP_SLOT) && sym_idx != 0 && sym_idx < sym_count && lsymtab && lstrtab) {
						*loc = lsymtab[sym_idx].st_value + load_addr;
					}
				}
			}

			free(ldyns);
			break;
		}
		free(phdr);
	}

	free(hdr);
	/* Return the address just past the end of the loaded image */
	return load_addr + (max_end - first_vaddr);
}
