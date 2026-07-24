CC=i686-elf-gcc
LD=i686-elf-ld
CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie

all_apps_src	:= $(shell find . -maxdepth 1 -name "*.c" ! -name "test_dynlib.c")
apps_bins	=  $(patsubst ./%.c, %.bin, $(all_apps_src))


all: $(apps_bins) test_dynlib.bin

clean:
	rm -f *.o *.bin

../libc/build/libc.o:
	make -C ../libc build/libc.o

../libc/build/libc.so:
	make -C ../libc build/libc.so

%.bin: %.c ../libc/build/libc.o
	@echo "[BUILD] $@"
	$(CC) -I../libc/src/include $(CC_FLAGS) -c -o $(subst .bin,.o,$@) $(subst .bin,.c,$@)
	$(LD) -m elf_i386 -T linker.ld -o $@ $(subst .bin,.o,$@) ../libc/build/libc.o

test_dynlib.bin: test_dynlib.c ../libc/build/libc.so
	@echo "[BUILD-DYN] $@"
	nasm -f elf -o /tmp/dyn_crt0.o ../libc/src/crt0.asm
	$(CC) -fPIC -fpie -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -I../libc/src/include -c -o test_dynlib.c.o test_dynlib.c
	$(LD) -shared -T dynamic.ld -o $@ /tmp/dyn_crt0.o test_dynlib.c.o
	rm -f test_dynlib.c.o /tmp/dyn_crt0.o
