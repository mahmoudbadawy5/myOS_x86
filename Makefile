kernel_src = $(shell find kernel/src -name "*.c") $(shell find kernel/src -name "*.asm")

all: myos.iso

kernel.bin: $(kernel_src)
	make -C kernel

apps/test1.bin: apps/test1.c
	make -C apps test1.bin

initrd: tools/create_initrd.py apps/test1.bin
	@cp apps/test1.bin initrd/
	@python tools/create_initrd.py initrd initrd.img

myos.iso: kernel.bin initrd
	@cp kernel/kernel.bin isodir/boot
	@cp initrd.img isodir/boot
	grub-mkrescue -o myos.iso isodir

clean:
	make -C kernel clean
	rm myos.iso

run: myos.iso
	kvm -cdrom myos.iso