// Most of the code from: https://wiki.osdev.org/ELF_Tutorial

#pragma once

#include <types.h>
#include <proc/process.h>

typedef uint16_t Elf32_Half;	// Unsigned half int
typedef uint32_t Elf32_Off;	    // Unsigned offset
typedef uint32_t Elf32_Addr;	// Unsigned address
typedef uint32_t Elf32_Word;	// Unsigned int
typedef int32_t  Elf32_Sword;	// Signed int


enum Elf_Ident {
	EI_MAG0		= 0, // 0x7F
	EI_MAG1		= 1, // 'E'
	EI_MAG2		= 2, // 'L'
	EI_MAG3		= 3, // 'F'
	EI_CLASS	= 4, // Architecture (32/64)
	EI_DATA		= 5, // Byte Order
	EI_VERSION	= 6, // ELF Version
	EI_OSABI	= 7, // OS Specific
	EI_ABIVERSION	= 8, // OS Specific
	EI_PAD		= 9  // Padding
};

enum Elf_Type {
	ET_NONE		= 0, // Unknown Type
	ET_REL		= 1, // Relocatable File
	ET_EXEC		= 2, // Executable File
	ET_DYN		= 3  // Shared object / PIE executable
};


#define ELFMAG0	0x7F // e_ident[EI_MAG0]
#define ELFMAG1	'E'  // e_ident[EI_MAG1]
#define ELFMAG2	'L'  // e_ident[EI_MAG2]
#define ELFMAG3	'F'  // e_ident[EI_MAG3]
#define EM_386		(3)  // x86 Machine Type
#define EV_CURRENT	(1)  // ELF Current Version
#define ELFDATA2LSB	(1)  // Little Endian
#define ELFCLASS32	(1)  // 32-bit Architecture


#define ELF_NIDENT	16

typedef struct {
	uint8_t		e_ident[ELF_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} Elf32_Phdr;

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;


#define PT_LOAD 1
#define PT_DYNAMIC 2


/* ELF Dynamic section entry */
typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word  d_val;
		Elf32_Addr  d_ptr;
	} d_un;
} Elf32_Dyn;

/* Dynamic section tags */
#define DT_NULL     0
#define DT_NEEDED   1
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_JMPREL   23
#define DT_PLTRELSZ 2

/* ELF Symbol table entry */
typedef struct {
	Elf32_Word    st_name;
	Elf32_Addr    st_value;
	Elf32_Word    st_size;
	uint8_t       st_info;
	uint8_t       st_other;
	Elf32_Half    st_shndx;
} Elf32_Sym;

#define ELF32_ST_BIND(info) ((info) >> 4)
#define ELF32_ST_TYPE(info) ((info) & 0xF)
#define STT_FUNC    2
#define STT_OBJECT  1
#define STB_GLOBAL  1
#define STB_WEAK    2
#define SHN_UNDEF   0

/* ELF Relocation entry (Rel) */
typedef struct {
	Elf32_Addr    r_offset;
	Elf32_Word    r_info;
} Elf32_Rel;

/* ELF Relocation entry with addend (Rela) */
typedef struct {
	Elf32_Addr    r_offset;
	Elf32_Word    r_info;
	Elf32_Sword   r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(info)  ((info) >> 8)
#define ELF32_R_TYPE(info) ((uint8_t)(info))

/* i386 relocation types */
#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT   7
#define R_386_RELATIVE  8

/* Shared library base address (mapped here for all processes) */
#define SHLIB_BASE 0x40000000

bool elf_check_file(Elf32_Ehdr *hdr);
bool elf_check_supported(Elf32_Ehdr *hdr);
int load_elf(pcb_t* proc, const char* path);
int load_shared_library(pcb_t *proc, const char *path, uint32_t load_addr);
