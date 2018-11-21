
%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
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


    .e801_failed_so_try88:
	mov ah, 0x88
	int 0x15
	jc .error_hlt
	and eax, 0x0000FFFF

	mov cx, 0x400
	mul cx
	shl edx, 16
	or edx, eax
	add edx, 0x100000

    .mem_get_ok:
	mov [total_mem_bytes], edx


;****准备进入保护模式****
	;打开A20
	in al, 0x92
	or al, 0000_0010B
	out 0x92, al

	;在gdtr寄存器中加载GDT的地址和偏移量
	lgdt [gdt_ptr]

	;将CR0寄存器的PE位变成1
	mov eax, cr0
	or eax, 0x00000001
	mov cr0, eax


	;刷新流水线
	jmp dword SELECTOR_CODE:p_mode_start

    .error_hlt:
	hlt

[bits 32]
p_mode_start:
	mov ax, SELECTOR_DATA
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, LOADER_STACK_TOP
	mov ax, SELECTOR_VIDEO
	mov gs, ax


	;加载kernal
	mov eax, KERNEL_START_SECTOR
	mov ebx, KERNEL_BIN_BASE_ADDR
	mov ecx, 200

	call rd_disk_m_32
	
	;创建页目录及页表并初始化页内存位图
	call setup_page
	
	sgdt [gdt_ptr]

	;将gdt描述符中视频段描述符段基址加0xc0000000，也就是把它挪到内核高地址区
	mov ebx, [gdt_ptr + 2]
	;视频段描述符是第三个(序号从0开始)
	;而且段描述符高4字节的最高字节是段基址的31-24位
	or dword [ebx + 0x18 + 4], 0xc0000000

	;将gdt的基址和栈指针同样映射到内核地址
	add dword [gdt_ptr + 2], 0xc0000000
	add esp, 0xc0000000

	;将页目录地址赋给cr3
	mov eax, PAGE_DIR_TABLE_POS
	mov cr3, eax
	;打开cr0的pg位
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax

	;重新加载gdt_ptr
	lgdt [gdt_ptr]
	

	mov byte [gs:160], 'V'

	jmp $


;****创建页目录及页表****
setup_page:
	;先把页目录占用的空间逐字清零
	;页目录表占据一页4kB，作者逐字节清零
	mov ecx, 4096
	mov esi, 0
    .clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS + esi], 0
	inc esi
	loop .clear_page_dir
	
    	;给页目录表创建页目录项
    .create_pde:
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x1000
	mov ebx, eax    
	;设置为任何特权级都可访问，且可写
	or eax, PG_US_U | PG_RW_W | PG_P
	;将第0个和第768个页目录项设置为第一个页表的位置(0x101000)及上一行的属性
	mov [PAGE_DIR_TABLE_POS + 0x0], eax
	mov [PAGE_DIR_TABLE_POS + 0xc00], eax
	;将最后一个页目录项设置为页目录表自己的位置
	sub eax, 0x1000
	mov [PAGE_DIR_TABLE_POS + 4092], eax
	
	;创建页表项PTE
	;因为先只创建loader所在的1MB空间的页表，一共256个
	mov ecx, 256
	mov esi, 0
	mov edx, PG_US_U | PG_RW_W | PG_P

    .create_pte:
	mov [ebx+esi*4], edx
	add edx, 4096
	inc esi
	loop .create_pte

	;创建内核其他页表的PDE
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x2000
	or eax, PG_US_U | PG_RW_W | PG_P
	mov ebx, PAGE_DIR_TABLE_POS
	mov ecx, 254
	mov esi, 769
    .create_kernel_pde:
	mov [ebx+esi*4], eax
	inc esi
	add eax, 0x1000
	loop .create_kernel_pde
	
	ret
	
kernel_init:
	xor eax, eax
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	;使用dx来记录程序头的大小
	;使用ebx来记录第一个程序头的偏移量
	;使用cx来记录有几个程序头
	mov dx, [KERNEL_BIN_BASE_ADDR + 42]
	mov ebx, [KERNEL_BIN_BASE_ADDR + 28]
	add ebx, KERNEL_BIN_BASE_ADDR
	mov cx, [KERNEL_BIN_BASE_ADDR + 44]

    
    .each_segment:
	;查看该程序头是否被使用
	cmp byte [ebx + 0], PT_NULL
	je .PTNULL

	;为函数mem_cpy压入参数，参数从左往右依然压入
	;函数原型类似于mem_cpy(dst, src, size)
	;先压入size，它位于程序头中偏移量16的位置
	push dword [ebx + 16]

	mov eax, [ebx + 4]
	add eax, KERNEL_BIN_BASE_ADDR
	;压入mem_cpy的第二个参数源地址，第一个参数目的地址
	push eax
	push dword [ebx + 8]
	
	call mem_cpy
	add esp, 12

    .PTNULL:
	add ebx, edx
	loop .each_segment
	ret


;逐字节拷贝函数mem_cpy, 三个参数dst, src, size
mem_cpy:
	cld
	push ebp
	mov ebp, esp
	;因为之后要使用到ecx，所以先备份
	push ecx
	mov edi, [ebp + 8]	;目的地址
	mov esi, [ebp + 12]  	;源地址
	mov ecx, [ebp + 16] 	;多少个
	rep movsb

	;把ecx、ebp恢复
	pop ecx
	pop ebp

	ret
	

















