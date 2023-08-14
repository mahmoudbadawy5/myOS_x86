kernel_src = $(shell find kernel/src -name "*.c") $(shell find kernel/src -name "*.asm")

all: myos.iso

kernel.bin: $(kernel_src)
	make -C kernel

initrd: tools/create_initrd.py
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