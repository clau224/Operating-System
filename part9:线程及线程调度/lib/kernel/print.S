
%include "print.inc"

section .data
put_int_buffer dq 0

[bits 32]
section .text
global put_char
global put_str
global put_int


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


put_str:
	;只用到ecx，ebx两个寄存器
	push ebx
	push ecx
	xor ecx, ecx
	mov ebx, [esp + 12]
    .goon:
	mov cl, [ebx]
	;判断是不是字符串的结尾'\0'
	cmp cl, 0
	jz .str_over
	;压入put_char的参数
	push ecx
	call put_char
	add esp, 4
	inc ebx
	jmp .goon
    .str_over:
	pop ecx
	pop ebx
	ret

put_int:
	;将所有的寄存器信息压栈保存
	pushad
	mov ebp, esp
	mov eax, [ebp + 36]
	mov edx, eax
	;edi表示在缓冲区第几个位置，0开始，0-7
	mov edi, 7
	;ecx存储循环次数
	mov ecx, 8
	mov ebx, put_int_buffer

    .16based_4bits:
	and edx, 0x0000000F
	cmp edx, 9
	;若比9大，则说明是由A-F表示的
	jg .is_AtoF
	;否则，加上'0'的ascii码，即为该位数字的ascii码
	add edx, '0'
	jmp .store
    .is_AtoF:
	sub edx, 10
	add edx, 'A'
    .store:
	;在缓冲区中从后向前依次存储
	mov [ebx + edi], dl
	dec edi
	;将eax右移4位，和0x0000000F相与获得后四位
	shr eax, 4
	mov edx, eax
	loop .16based_4bits

    .ready_to_print:
	;之前的循环中，已把edi减为了-1，所以要将其变为0
	inc edi
    .skip_prefix_0:
	;edi记录当前检查的数字的下角标，若为8，说明0-7都是0，所以这个数是0
	cmp edi, 8
	je .full0
    .goon_skip:
	mov cl, [put_int_buffer + edi]
	inc edi
	;检查是否为0
	cmp cl, '0'
	;若是继续检查下一位
	je .skip_prefix_0
	;需要将edi中的值减一，来表示第一个非零
	dec edi
	jmp .put_each_num

    .full0:
	mov cl, '0'
    .put_each_num:
	;此时，ecx中存放的是第一个非0的数字，edi存放的是该数字的下标
	push ecx
	call put_char
	add esp, 4
	inc edi
	mov cl, [put_int_buffer + edi]
	cmp edi, 8
	jl .put_each_num
	popad
	ret

global set_cursor
set_cursor:
   	pushad
   	mov bx, [esp+36]

	;索引寄存器
   	mov dx, 0x03d4		
	;光标位置的高8位	
   	mov al, 0x0e		
   	out dx, al
	;通过读写数据端口0x3d5来获得或设置光标位置
   	mov dx, 0x03d5			 
   	mov al, bh
   	out dx, al

   	mov dx, 0x03d4
	;再设置低8位
   	mov al, 0x0f
   	out dx, al
   	mov dx, 0x03d5 
   	mov al, bl
   	out dx, al
   	popad
   	ret





