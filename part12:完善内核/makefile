BUILD_DIR = ./build

ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc -m32
LD = ld -m elf_i386

LIB = -I lib/ -I lib/kernel/ -I kernel/ -I device/ -I thread/ -I userprog/ \
		-I lib/user
LIB_PRINT = -I lib/kernel/include/
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
      	$(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
      	$(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o \
	  	$(BUILD_DIR)/memory.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o \
	  	$(BUILD_DIR)/list.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o \
	  	$(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o \
	  	$(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/stdio.o \
	  	$(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/stdio.o

ASFLAGS = -f elf
CFLAGS = $(LIB) -c -fno-builtin -fno-stack-protector
LDFLAGS = -Ttext $(ENTRY_POINT) -e main


$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h lib/stdint.h \
	kernel/init.h kernel/memory.h thread/thread.h kernel/interrupt.h \
	device/console.h device/keyboard.h device/ioqueue.h \
	userprog/process.h userprog/syscall-init.h lib/user/syscall.h \
	lib/stdio.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h lib/kernel/print.h \
	lib/stdint.h kernel/interrupt.h device/timer.h device/console.h \
	device/keyboard.h userprog/tss.h userprog/syscall-init.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h \
	lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/stdint.h \
        lib/kernel/io.h lib/kernel/print.h kernel/interrupt.h \
		thread/thread.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h \
        lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/string.o: lib/string.c lib/string.h kernel/global.h \
		kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h \
		lib/stdint.h lib/string.h lib/kernel/print.h kernel/interrupt.h \
		kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h lib/string.h \
		lib/stdint.h lib/kernel/print.h kernel/debug.h thread/sync.h \
		thread/thread.h lib/kernel/bitmap.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h lib/stdint.h \
		kernel/global.h kernel/memory.h lib/string.h lib/kernel/print.h \
		kernel/debug.h kernel/interrupt.h userprog/process.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h lib/kernel/list.h \
		kernel/global.h kernel/debug.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/console.o:device/console.c device/console.h lib/kernel/print.h \
		lib/stdint.h thread/sync.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h lib/kernel/print.h \
		kernel/interrupt.h lib/kernel/io.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/ioqueue.o: device/ioqueue.c device/ioqueue.h kernel/interrupt.h \
		kernel/global.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h lib/stdint.h kernel/global.h \
		lib/string.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h kernel/global.h \
		kernel/debug.h kernel/memory.h thread/thread.h lib/kernel/list.h \
		userprog/tss.h kernel/interrupt.h lib/string.h device/console.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/syscall-init.o: userprog/syscall-init.c userprog/syscall-init.h \
		lib/user/syscall.h lib/stdint.h lib/kernel/print.h thread/thread.h \
		device/console.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h kernel/interrupt.h \
		lib/user/syscall.h lib/kernel/print.h kernel/global.h lib/string.h 
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.S lib/kernel/include/print.inc
	$(AS) $(ASFLAGS) $(LIB_PRINT) $< -o $@
$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@


$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@


mk_dir:
	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR);fi

hd:
	dd if=$(BUILD_DIR)/kernel.bin of=hd60M.img \
		bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: 	$(BUILD_DIR)/kernel.bin

all:	mk_dir build hd







