#!/bin/bash

bximage -hd -mode="flat" -size=60 -q hd60M.img

nasm -I include/ -o mbr.bin mbr.S

nasm -I include/ -o loader.bin loader.S

dd if=mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=loader.bin of=hd60M.img bs=512 count=2 seek=2 conv=notrunc
