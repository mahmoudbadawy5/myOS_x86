c_src	:= $(shell find src -name "*.c")
asm_src	:= $(shell find src -name "*.asm") 
c_obj	=  $(patsubst src/%, build/%, $(c_src:.c=.o))
asm_obj	=  $(patsubst src/%, build/%, $(asm_src:.asm=.o))
all_obj = $(c_obj) $(asm_obj) build/arch/isr.o build/arch/irq.o build/arch/isr_asm.o build/arch/irq_asm.o

CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie

includes = "src/include"

all: kernel.bin

kernel.bin: $(all_obj)
	@echo "[LD] $@"
	@echo "Linking $(all_obj)"
	@rm -f kernel.bin
	@ld -m elf_i386 -T link.ld -o $@ $(all_obj)

build/%.o : src/%.asm
	@echo "[ASM] $@"
	@mkdir -p $(shell dirname $@)
	@nasm -f elf -o $@ $<

build/%.o : src/%.c
	@echo "[CPP] $@"
	@mkdir -p $(shell dirname $@)
	@gcc -I$(includes) $(CC_FLAGS)  -c -o $@ $<

build/%.o : gen_src/%.asm
	@echo "[ASM] $@"
	@mkdir -p $(shell dirname $@)
	@nasm -f elf -o $@ $<

build/%.o : gen_src/%.c
	@echo "[CPP] $@"
	@mkdir -p $(shell dirname $@)
	@gcc -I$(includes) $(CC_FLAGS) -c -o $@ $<


run: kernel.bin
	@kvm -kernel kernel.bin

clean:
	@rm -f kernel.bin
	@rm -rf gen_src
	@rm -rf build


gen_src/arch/isr_asm.asm: tools/isr_asm_gen.py
	@mkdir -p gen_src/arch
	@python tools/isr_asm_gen.py > gen_src/arch/isr_asm.asm

gen_src/arch/isr.c: tools/isr_c_gen.py
	@mkdir -p gen_src/arch
	@python tools/isr_c_gen.py > gen_src/arch/isr.c

gen_src/arch/irq_asm.asm: tools/irq_asm_gen.py
	@mkdir -p gen_src/arch
	@python tools/irq_asm_gen.py > gen_src/arch/irq_asm.asm

gen_src/arch/irq.c: tools/irq_c_gen.py
	@mkdir -p gen_src/arch
	@python tools/irq_c_gen.py > gen_src/arch/irq.c