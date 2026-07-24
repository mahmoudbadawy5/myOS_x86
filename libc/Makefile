CC=i686-elf-gcc
LD=i686-elf-ld
CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie
PIC_FLAGS = -fPIC -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

c_src	:= $(shell find src -name "*.c")
asm_src	:= $(shell find src -name "*.asm")
c_obj   =  $(patsubst src/%, build/%, $(c_src:.c=.o))
c_pic_obj = $(patsubst src/%, build_pic/%, $(c_src:.c=.o))
asm_obj	=  $(patsubst src/%, build/%, $(asm_src:.asm=.o))
all_obj	=  $(c_obj) $(asm_obj)

all: build/libc.o build/libc.so

clean:
	@rm -rf build build_pic

build/libc.o: $(all_obj)
	@echo "[LD] $@"
	@mkdir -p build
	@$(LD) -z noexecstack -m elf_i386 -r -o build/libc.o $(all_obj)

build/libc.so: $(c_pic_obj)
	@echo "[LD-SO] $@"
	@mkdir -p build
	@$(LD) -shared -soname libc.so -m elf_i386 -o $@ $(c_pic_obj)

build/%.o : src/%.c
	@echo "[CPP] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CC_FLAGS)  -Isrc/include -c -o $@ $<

build_pic/%.o : src/%.c
	@echo "[CPP-PIC] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(PIC_FLAGS) -Isrc/include -c -o $@ $<

build/%.o : src/%.asm
	@echo "[ASM] $@"
	@mkdir -p $(shell dirname $@)
	@nasm -f elf -o $@ $<
