CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie
c_src	:= $(shell find src -name "*.c")
asm_src	:= $(shell find src -name "*.asm")
c_obj   =  $(patsubst src/%, build/%, $(c_src:.c=.o))
asm_obj	=  $(patsubst src/%, build/%, $(asm_src:.asm=.o))
all_obj	=  $(c_obj) $(asm_obj)

all: build/libc.o

clean:
	@rm -rf build

build/libc.o: $(all_obj)
	@echo "[LD] $@"
	@mkdir -p build
	@ld -z noexecstack -m elf_i386 -r -o build/libc.o $(all_obj)

build/%.o : src/%.c
	@echo "[CPP] $@"
	@mkdir -p $(shell dirname $@)
	@gcc $(CC_FLAGS)  -Isrc/include -c -o $@ $<

build/%.o : src/%.asm
	@echo "[ASM] $@"
	@mkdir -p $(shell dirname $@)
	@nasm -f elf -o $@ $<