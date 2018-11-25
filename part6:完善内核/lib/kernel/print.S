
%include "print.inc"

[bits 32]
section .text
global put_char
put_char:
	;先备份32位寄存器环境
	pushad
	;将视频段选择子放入gs中
	mov ax, SELECTOR_VIDEO
	mov gs, ax

	;获取当前光标位置
	;先获取高八位
	mov dx, 0x03d4
	mov al, 0x0e
	out dx, al
	mov dx, 0x03d5
	in al, dx
	mov ah, al

	;再获取低八位
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	in al, dx

	;将光标存入bx
	mov bx, ax
	;在栈中获取待打印的字符
	mov ecx, [esp + 36]

	;假如是回车符CR
	cmp cl, 0xd
	jz .is_carriage_return
	
	;假如是换行符LF
	cmp cl, 0xa
	jz .is_line_feed

	;假如输入的是backspace
	cmp cl, 0x8
	jz .is_backspace
	
	;否则是其他字符的话
	jmp .put_other


    ;若是回车符CR或换行符LF的话，现根据当前坐标求得当前行首坐标
    ;对于回车符CR，求得下一行行首坐标，同时检查是否超出屏幕
    ;对于换行符LF，光标加80即可
    .is_line_feed:
    .is_carriage_return:
	xor dx, dx
	mov ax, bx
	mov si, 80
	div si
	sub bx, dx
    .is_carriage_return_end:
	add bx, 80
	cmp bx, 2000
    .is_line_feed_end:
	jl .set_cursor

    .is_backspace:
	dec bx
	shl bx, 1
	;写入‘空格’的ascii码
	mov byte [gs:bx], 0x20
	inc bx
	mov byte [gs:bx], 0x07
	shr bx, 1
	jmp .set_cursor

    .put_other:
	shl bx, 1
	mov [gs:bx], cl
	inc bx
	mov byte [gs:bx], 0x07
	shr bx, 1
	inc bx
	cmp bx, 2000
	jl .set_cursor


    
    .roll_screen:
	cld
	mov ecx, 960
	;循环将第1到24行的数据搬运到0到23行
	mov esi, 0xc00b80a0
	mov edi, 0xc00b8000
	rep movsd
	;将最后一行填充为空白
	mov ebx, 3840
	mov ecx, 80
    .cls:
	mov word [gs:ebx], 0x720
	add ebx, 2
	loop .cls
	mov bx, 1920

    .set_cursor:
	mov dx, 0x03d4
	mov al, 0x0e
	out dx, al
	mov dx, 0x03d5
	mov al, bh
	out dx, al

	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	mov al, bl
	out dx, al
	
    .put_char_done:
	popad
	ret




