#!/bin/bash

bximage -hd -mode="flat" -size=60 -q hd60M.img

gcc -c -o kernel/main.o kernel/main.c

ld kernel/main.o -Ttext 0xc0001500 -e main -o kernel/kernel.bin

nasm -I include/ -o mbr.bin mbr.S

nasm -I include/ -o loader.bin loader.S

dd if=mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

dd if=kernel/kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc
