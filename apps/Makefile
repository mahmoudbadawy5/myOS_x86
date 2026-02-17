CC_FLAGS = -fno-stack-protector -m32 -Wall -O -fstrength-reduce -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -fno-pie

all_apps_src	:= $(shell find src -name "*.c")
apps_bins	=  $(patsubst src/%, build/%, $(all_apps_src:.c=.bin))


all: $(apps_bins)

clean:
	rm -f *.o *.bin

../libc/build/libc.o:
	make -C ../libc build/libc.o

%.bin: %.c ../libc/build/libc.o
	@echo "[BUILD] $@"
	gcc -I../libc/src/include $(CC_FLAGS) -c -o $(subst .bin,.o,$@) $(subst .bin,.c,$@) 
	ld -m elf_i386 -T linker.ld -o $@ $(subst .bin,.o,$@) ../libc/build/libc.o
