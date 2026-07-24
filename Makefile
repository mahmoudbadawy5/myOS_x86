kernel_src = $(shell find kernel/src -name "*.c") $(shell find kernel/src -name "*.asm")
apps = $(patsubst %.c, %.bin, $(shell find apps -name "*.c"))

all: myos.iso

kernel.bin: $(kernel_src)
	make -C kernel

apps/%.bin: apps/%.c
	make -C apps $(patsubst apps/%, %, $@)

initrd: tools/create_initrd.py $(apps)
	@cp $(apps) initrd/
	@cp libc/build/libc.so initrd/
	@python tools/create_initrd.py initrd initrd.img

.PHONY: fat12

fat12: tools/create_fat12.sh
	@bash tools/create_fat12.sh

myos.iso: kernel.bin initrd
	@cp kernel/kernel.bin isodir/boot
	@cp initrd.img isodir/boot
	grub-mkrescue -o myos.iso isodir

clean:
	make -C kernel clean
	make -C libc clean
	make -C apps clean
	rm -f initrd/*.bin
	rm -f myos.iso
	rm -f fat12.img

run: myos.iso fat12
	kvm -cdrom myos.iso -drive file=fat12.img,format=raw,if=ide,index=0,media=disk -boot d

bochs: myos.iso fat12
	bochs
