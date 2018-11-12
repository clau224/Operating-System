
%include "boot.inc"


;本程序在编译时，起始地址为0x7c00
SECTION MBR vstart=0x7c00
	;用cs段寄存器的值去初始化其他寄存器的值
	mov ax, cx
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	;然后初始化栈指针，因为0x7c00以下暂时安全区域
	mov sp, 0x7c00

	mov ax, 0xb800
	mov gs, ax


;清屏，利用0x06号功能，上卷全部行，即可实现清屏功能
;AH为功能号，即0x06， AL为上卷行数，当其为0时，表示全部，由此ax可设为0x600
;BH为上卷行属性，在这里我们设为0x07，bx值为0x700
;CL、CH依次代表左上角横纵坐标，因此cx为0
;DL、DH依次代表右下角横纵坐标，因为VGA文本模式中，一行80字符，共25行
;故坐标为(79,24)，即dx的值为0x184f
	mov ax, 0x600
	mov bx, 0x700
	mov cx, 0
	mov dx, 0x184f

	int 0x10


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;以上与part2部分相同;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


; 输出背景色绿色，前景色红色，并且跳动的字符串"1 MBR"
; 低八位为字符的ascii码，高八位的前四位控制背景色，后四位控制前景色
	mov byte [gs:0x00], '1'
	mov byte [gs:0x01], 0xA4

	mov byte [gs:0x02], ' '
	mov byte [gs:0x03], 0xA4

	mov byte [gs:0x04], 'M'
	mov byte [gs:0x05], 0xA4
	
	mov byte [gs:0x06], 'B'
	mov byte [gs:0x07], 0xA4

	mov byte [gs:0x08], 'R'
	mov byte [gs:0x09], 0xA4

	;起始扇区lba地址
	mov eax, LOADER_START_SECTOR
	;写入的地址
	mov bx, LOADER_BASE_ADDR
	;待读入的扇区数
	mov cx, 4
	;读取程序的起始部分
	call rd_disk_m_16

	jmp LOADER_BASE_ADDR



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;功能：读取硬盘n个扇区;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


rd_disk_m_16:
	mov esi, eax
	mov di, cx
;读写硬盘
;设置要读取的扇区数
	mov dx, 0x1f2
	mov al, cl
	out dx, al

;恢复eax
	mov eax, esi

;将LBA地址存入0x1f3 ~ 0x1f6
	mov dx, 0x1f3
	out dx, al

	mov cl, 8
	shr eax, cl
	mov dx, 0x1f4
	out dx, al

	shr eax, cl
	mov dx, 0x1f5
	out dx, al

	shr eax, cl
	and al, 0x0f
	or al, 0xe0
	mov dx, 0x1f6
	out dx, al

;向0x1f7端口写入读命令0x20
	mov dx, 0x1f7
	mov al, 0x20
	out dx, al

;检测硬盘状态
    .not_ready:
;同一端口，写时表示写入命令字，读时表示读入硬盘状态
	nop
	in al, dx
;第三位为1表示硬盘控制器已准备好数据传输，第七位为1表示硬盘忙
	and al, 0x88
	cmp al, 0x08
	jnz .not_ready


;从0x1f0端口读取数据
;di为要读取的扇区数，一个扇区有512字节，每次读入两个字节，即di×256次
	mov ax, di
	mov dx, 256
	mul dx
	mov cx, ax
	mov dx, 0x1f0
    .go_on_read:
	in ax, dx
	mov [bx], ax
	add bx, 2
	loop .go_on_read
	
	ret

    times 510-($-$$) db 0
    db 0x55, 0xaa



















