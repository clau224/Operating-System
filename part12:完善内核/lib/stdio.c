#include "stdio.h"
#include "interrupt.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "print.h"

#define va_start(ap, v) ap = (va_list)&v 
#define va_arg(ap, t) *((t*)(ap += 4))
#define va_end(ap) ap = NULL

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

/* 将整型转换成字符(integer to ascii) */
/*static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
   uint32_t m = value % base;	    // 求模,最先掉下来的是最低位   
   uint32_t i = value / base;	    // 取整
   if (i) {			    // 如果倍数不为0则递归调用。
      itoa(i, buf_ptr_addr, base);
   }
   if (m < 10) {      // 如果余数是0~9
      *((*buf_ptr_addr)++) = m + '0';	  // 将数字0~9转换为字符'0'~'9'
   } else {	      // 否则余数是A~F
      *((*buf_ptr_addr)++) = m - 10 + 'A'; // 将数字A~F转换为字符'A'~'F'
   }
}*/

uint32_t vsprintf(char* str, const char* format, va_list ap){
	char* str_ = str;
	const char* format_ = format;
	char index_char = *format_;
	int32_t arg_int;
	while(index_char){
		if(index_char != '%'){
			*(str_) = index_char;
			index_char = *(++format_);
			str_++;
			continue;
		}
		index_char = *(++format_);
		switch(index_char){
			case 'x':
				arg_int = va_arg(ap, int);
				itoa(arg_int, &str_, 16);
				index_char = *(++format_);
				break;
		}
	}
	return strlen(str);
}


uint32_t printf(const char* format, ...){
	va_list args;
	va_start(args, format);
	char buf[1024] = {0};
	vsprintf(buf, format, args);
	va_end(args);
	return write(buf);
}
