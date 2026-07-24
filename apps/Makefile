CC=i686-elf-gcc
LD=i686-elf-ld
DYN_FLAGS = -fPIC -fpie -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin

all_apps_src := $(shell find . -maxdepth 1 -name "*.c" ! -name "test_dynlib.c")
apps_bins    = $(patsubst ./%.c, %.bin, $(all_apps_src))

all: $(apps_bins) test_dynlib.bin

clean:
	rm -f *.o *.bin

../libc/build/libc.so:
	make -C ../libc build/libc.so

../libmath/build/libmath.so:
	make -C ../libmath

%.bin: %.c ../libc/build/libc.so
	@echo "[BUILD-DYN] $@"
	nasm -f elf -o /tmp/dyn_crt0.o ../libc/src/crt0.asm
	$(CC) -I../libc/src/include $(DYN_FLAGS) -c -o $(subst .bin,.o,$@) $(subst .bin,.c,$@)
	$(LD) -shared -T dynamic.ld -o $@ /tmp/dyn_crt0.o $(subst .bin,.o,$@) ../libc/build/libc.so
	rm -f $(subst .bin,.o,$@) /tmp/dyn_crt0.o

test_dynlib.bin: test_dynlib.c ../libc/build/libc.so ../libmath/build/libmath.so
	@echo "[BUILD-DYN] $@"
	nasm -f elf -o /tmp/dyn_crt0.o ../libc/src/crt0.asm
	$(CC) -I../libc/src/include -I../libmath/include $(DYN_FLAGS) -c -o test_dynlib.o test_dynlib.c
	$(LD) -shared -T dynamic.ld -o $@ /tmp/dyn_crt0.o test_dynlib.o ../libc/build/libc.so ../libmath/build/libmath.so
	rm -f test_dynlib.o /tmp/dyn_crt0.o
