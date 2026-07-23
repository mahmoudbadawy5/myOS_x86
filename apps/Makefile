CC=i686-elf-gcc
LD=i686-elf-ld
CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie

all_apps_src	:= $(shell find . -maxdepth 1 -name "*.c")
apps_bins	=  $(patsubst ./%.c, %.bin, $(all_apps_src))


all: $(apps_bins)

clean:
	rm -f *.o *.bin

../libc/build/libc.o:
	make -C ../libc build/libc.o

%.bin: %.c ../libc/build/libc.o
	@echo "[BUILD] $@"
	$(CC) -I../libc/src/include $(CC_FLAGS) -c -o $(subst .bin,.o,$@) $(subst .bin,.c,$@)
	$(LD) -m elf_i386 -T linker.ld -o $@ $(subst .bin,.o,$@) ../libc/build/libc.o
