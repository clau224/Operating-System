
%include "boot.inc"

section loader vstart = LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR


GDT_BASE:  dd    0x00000000
	    dd    0x00000000

CODE_DESC:  dd    0x0000FFFF
	       dd    DESC_CODE_HIGH4

DATA_STACK_DESC:  dd    0x0000FFFF
		  dd    DESC_DATA_HIGH4

VIDEO_DESC:  dd    0x80000007
	         dd    DESC_VIDEO_HIGH4

GDT_SIZE equ $-GDT_BASE
GDT_LIMIT equ GDT_SIZE-1
times 60 dq 0

SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

;total_mem_bytes 相对于本文件头部的偏移量为0x200，因为前面有4+60个8字节描述符
total_mem_bytes dd 0

gdt_ptr dw GDT_LIMIT
         dd GDT_BASE

;作者为了人工对齐，使loader_start的偏移地址凑成为0x300
ards_buf times 244 db 0
ards_nr dw 0

loader_start:
	;初始化ebx为0
	xor ebx, ebx
	mov edx, 0x534d4150
	;ards结构缓冲区
	mov di, ards_buf
    .e820_mem_get_loop:
	;因为每次执行完int 0x15后，eax的值都会改变，所以放在循环中
	mov eax, 0x0000e820
	mov ecx, 20
	int 0x15
	;若e820子功能失败，则尝试e801
	jc .e820_failed_so_try_e801
	add di, cx
	inc word [ards_nr]
	;ebx为0说明是最后一个ARDS结构
	cmp ebx, 0
	jnz .e820_mem_get_loop

	;找出基地址+内存长度的最大值;
	mov cx, [ards_nr]
	mov ebx, ards_buf
	;edx存储最大的内存容量
	xor edx, edx
    .find_max_mem_area:
	;基地址是ARDS结构体前八位，内存长度是ARDS随后八位
	mov eax, [ebx]
	add eax, [ebx+8]
	add ebx, 20
	;比较存储的最大值和当前值
	cmp edx, eax
	jge .next_ards
	;若不是则不更新
	mov edx, eax
    .next_ards:
	loop .find_max_mem_area
	jmp .mem_get_ok
	

    .e820_failed_so_try_e801:
	mov ax, 0xe801
	int 0x15
	jc .e801_failed_so_try88
	;ax中存储着16MB以内有多少个1KB，bx中存储着4GB中有多少个64KB
	mov cx, 0x400
	mul cx
	shl edx, 16
	and eax, 0x0000FFFF
	or edx, eax
	add edx, 0x100000
	mov esi, edx
	
	xor eax, eax
	mov ax, bx
	mov ecx, 0x10000
	mul ecx
	;把两个结果相加
	add esi, eax
	mov edx, esi
	jmp .mem_get_ok































