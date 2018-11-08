
%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

jmp loader_start

;构建gdt及其内部的描述符
;上面的dd定义段描述符低4字节，下面的是高4字节
;以下分别为第0段描述符，代码段描述符，数据段及栈段描述符，显存段描述符
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

;以下定义的是全局描述符表的GDT的指针，此指针是lgdt加载GDT到gdrt寄存器用的
;lgdt格式，前16位是界限值，后32位是GDT起始地址
gdt_ptr  dw    GDT_LIMIT
	  dd    GDT_BASE

loadermsg db '2 loader in real.'

loader_start:
;int 0x10指令，需要设置AX,BX,CX,DX以及SP,BP寄存器
	mov sp, LOADER_BASE_ADDR
	mov bp, loadermsg
	mov cx, 17
	mov ax, 0x1301
	mov bx, 0x001f
	mov dx, 0x1800
	int 0x10
	

;准备进入保护模式
	;1,打开A20
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


[bits 32]
p_mode_start:
	mov ax, SELECTOR_DATA
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, LOADER_STACK_TOP
	mov ax, SELECTOR_VIDEO
	mov gs, ax

	mov byte [gs:160], 'P'

	jmp $


















