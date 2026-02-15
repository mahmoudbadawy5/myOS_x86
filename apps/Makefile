CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie

all: test1.bin test2.bin echo.bin

clean:
	rm -f *.o *.bin

../libc/build/libc.o:
	make -C ../libc build/libc.o

%.bin: %.c %.ld ../libc/build/libc.o
	@echo "[BUILD] $@"
	gcc -I../libc/src/include $(CC_FLAGS) -c -o $(subst .bin,.o,$@) $(subst .bin,.c,$@) 
	ld -m elf_i386 -T $(subst .bin,.ld,$@) -o $@ $(subst .bin,.o,$@) ../libc/build/libc.o
