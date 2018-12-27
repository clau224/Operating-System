#include "stdio.h"
#include "interrupt.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "print.h"

/*将整数转换为字符串，因为参数是无符号数，所以未判断符号*/
static void itoa(uint32_t value, char** buf, uint8_t base){
	uint32_t mod = value % base;
	value /= base;
	char* buf_origin = *buf;
	while(mod != 0 || value != 0){
		if(mod<10)
			*((*buf)++) = mod + '0';
		else
			*((*buf)++) = mod - 10 + 'A';
		mod = value % base;
		value /= base;
	}
	strrev(buf_origin, (*buf)-1);
}

//函数返回值是经过格式转换后的字符串长度
uint32_t vsprintf(char* str, const char* format, va_list ap){
	char* str_ = str;
	const char* format_ = format;
	char index_char = *format_;
	int32_t arg_int;
	char* arg_str;
	while(index_char){
		if(index_char != '%'){
			*(str_) = index_char;
			index_char = *(++format_);
			str_++;
			continue;
		}
		index_char = *(++format_);
		switch(index_char){
			//输出16进制
			case 'x':
				arg_int = va_arg(ap, int);
				itoa(arg_int, &str_, 16);
				break;
			//输出10进制
			case 'd':
				arg_int = va_arg(ap, int);
				if(arg_int < 0){
					arg_int = -arg_int;
					*str_++ = '-';
				}
				itoa(arg_int, &str_, 10);
				break;
			//输出字符
			case 'c':
				*(str_++) = va_arg(ap, char);
				break;
			//输出字符串
			case 's':
				arg_str = va_arg(ap, char*);
				strcpy(str_, arg_str);
				str_ += strlen(arg_str);
				break;
		}
		index_char = *(++format_);
	}
	return strlen(str);
}


uint32_t printf(const char* format, ...){
	va_list args;
	va_start(args, format);
	char buf[1024] = {0};
	vsprintf(buf, format, args);
	va_end(args);
	return write(1, buf, strlen(buf));
}

uint32_t sprintf(char* buf, const char* format, ...){
	va_list args;
	va_start(args, format);
	uint32_t retval = vsprintf(buf, format, args);
	va_end(args);
	return retval;
}
