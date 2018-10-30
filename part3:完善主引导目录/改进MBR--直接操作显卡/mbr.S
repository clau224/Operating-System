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

	jmp $

	times 510-($-$$) db 0
	db 0x55, 0xaa





























