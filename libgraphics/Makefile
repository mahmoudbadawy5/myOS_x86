CC=i686-elf-gcc
LD=i686-elf-ld
PIC_FLAGS = -fPIC -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

.PHONY: all clean

all: build/libgraphics.so

clean:
	@rm -rf build

build/libgraphics.so: build/graphics.o
	@echo "[LD-SO] $@"
	@mkdir -p build
	@$(LD) -shared -soname libgraphics.so -m elf_i386 -o $@ $<

build/%.o : src/%.c
	@echo "[CPP-PIC] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(PIC_FLAGS) -I../libc/src/include -Iinclude -c -o $@ $<
