#!/bin/bash

rm -f *.img
rm -f *.img.lock

bximage -hd -mode="flat" -size=60 -q hd60M.img

while true; do
read -p "please input the path of hd80M.img you have prepared: " your_path
if [ ! -f $your_path ]; then
	echo "文件不存在"
else
	break
fi
done
while true; do
read -p "please input the path of hd80M.img that you have set in bochserc.txt : " target_path
if [ ! -f $your_path ]; then
	echo "文件不存在"
else
	break
fi
done
cp -i $your_path $target_path

nasm -I boot/include/ -o build/mbr.bin boot/mbr.S

nasm -I boot/include/ -o build/loader.bin boot/loader.S

dd if=build/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc

dd if=build/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

make clean

make all
