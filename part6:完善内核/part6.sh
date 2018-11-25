#!/bin/bash

bximage -hd -mode="flat" -size=60 -q hd60M.img

nasm -I include/ -o mbr.bin mbr.S

nasm -I include/ -o loader.bin loader.S

nasm -f elf -I lib/kernel/include/ -o lib/kernel/print.o lib/kernel/print.S

gcc -m32 -I lib/kernel/ -c -o kernel/main.o kernel/main.c

ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel/kernel.bin kernel/main.o lib/kernel/print.o

dd if=mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

dd if=kernel/kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc
