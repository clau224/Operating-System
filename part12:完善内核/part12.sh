#!/bin/bash

rm -f *.img

bximage -hd -mode="flat" -size=60 -q hd60M.img

nasm -I boot/include/ -o build/mbr.bin boot/mbr.S

nasm -I boot/include/ -o build/loader.bin boot/loader.S

dd if=build/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=build/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

make clean

make all
