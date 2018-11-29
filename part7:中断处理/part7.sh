#!/bin/bash

bximage -hd -mode="flat" -size=60 -q hd60M.img

nasm -I boot/include/ -o build/mbr.bin boot/mbr.S

nasm -I boot/include/ -o build/loader.bin boot/loader.S

nasm -f elf -I lib/kernel/include/ -o build/print.o lib/kernel/print.S

nasm -f elf -o build/kernel.o kernel/kernel.S

gcc -m32 -I kernel/ -I lib/kernel/ -c -fno-builtin -o build/main.o kernel/main.c

gcc -m32 -I kernel/ -I lib/ -I lib/kernel -c -fno-builtin -fno-stack-protector -o build/interrupt.o kernel/interrupt.c

gcc -m32 -I kernel/ -I lib/kernel -c -fno-builtin -o build/init.o kernel/init.c

ld -m elf_i386 -Ttext 0xc0001500 -e main -o build/kernel.bin build/main.o \
	build/init.o build/print.o build/interrupt.o build/kernel.o

dd if=build/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=build/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

dd if=build/kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc
