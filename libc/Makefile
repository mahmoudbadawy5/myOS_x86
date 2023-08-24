CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie
c_src	:= $(shell find src -name "*.c")
c_obj   =  $(patsubst src/%, build/%, $(c_src:.c=.o))

all: build/libc.o

clean:
	@rm -rf build

build/libc.o: $(c_obj)
	@echo "[LD] $@"
	@mkdir -p build
	@ld -m elf_i386 -r -o build/libc.o $(c_obj)

build/%.o : src/%.c
	@echo "[CPP] $@"
	@mkdir -p $(shell dirname $@)
	@gcc $(CC_FLAGS)  -Isrc/include -c -o $@ $<
