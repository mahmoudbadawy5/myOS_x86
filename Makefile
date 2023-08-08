iso: myos.iso

kernel/kernel.bin:
	make -C kernel

myos.iso: kernel/kernel.bin
	@cp kernel/kernel.bin isodir/boot
	grub-mkrescue -o myos.iso isodir


clean:
	make -C kernel clean
	rm myos.iso

run: myos.iso
	kvm -cdrom myos.iso