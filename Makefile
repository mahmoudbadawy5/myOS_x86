kernel_src = $(shell find kernel/src -name "*.c") $(shell find kernel/src -name "*.asm")
apps = $(patsubst %.c, %.bin, $(shell find apps -name "*.c"))

all: myos.iso

kernel.bin: $(kernel_src)
	make -C kernel

apps/%.bin: apps/%.c
	make -C apps $(patsubst apps/%, %, $@)

initrd: tools/create_initrd.py $(apps)
	@cp $(apps) initrd/
	@python tools/create_initrd.py initrd initrd.img

myos.iso: kernel.bin initrd
	@cp kernel/kernel.bin isodir/boot
	@cp initrd.img isodir/boot
	grub-mkrescue -o myos.iso isodir

clean:
	make -C kernel clean
	make -C libc clean
	make -C apps clean
	rm initrd/*.bin
	rm myos.iso

run: myos.iso
	kvm -cdrom myos.iso