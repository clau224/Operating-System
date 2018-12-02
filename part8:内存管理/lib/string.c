#include "string.h"
#include "global.h"
#include "debug.h"

/*memset函数*/
void memset(void* dst_, uint8_t value, uint32_t size){
    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t*) dst_;
    while(size-->0){
		*dst++ = value;
    }
}

/*memcpy函数*/
void memcpy(void* dst_, const void* src_, uint32_t size){
	ASSERT(dst_ != NULL && src__ != NULL);
	uint8_t* dst = (uint8_t*)dst_;
	const uint8_t* src = src_;
	while(size-- > 0){
		*dst++ = *src++;
	}
}

/*memcmp函数*/
int strcmp(const void* a_, const void* b_, uint32_t size){
	ASSERT(a != NULL || b != NULL);
	const char *a = a_, *b = b_;
	while(size-- > 0){
		if(*a != *b){
			return *a>*b ? 1 : -1;
		}
		a++, b++;
	}
	return *a == 0 ? -1 : (*a == *b ? 0 : 1);
}

/*查找字符串中，第一个目标字符的位置,返回值是地址*/
char* strchr(const char* str, const uint8_t ch){
	ASSERT(str != NULL);
	while(*str != 0){
		if(*str == ch){
			return str;		
		}
		str++;
	}
}

/*这个是从后往前找*/
char* strrchr(const char* str, const uint8_t ch){
	ASSERT(str != NULL);
	const char* ansstr = NULL;
	while(*str != 0){
		if(*str == ch){
			ansstr = str;
		}
		str++;	
	}
	return ansstr;
}

/*strcat是拼接两个字符串，把src拼接到dst后面*/
char* strcat(char* dst_, const char* src_){
	ASSERT(dst_ != NULL && src_ != NULL);
	char* dst = dst_;
	while(*dst++);
	dst--;
	//这一步还是很巧妙地，先给dst指向的内存赋src指向的值val
	//然后dst和src同时向后移动，然后检查val是否为'\0'，不是则接着循环
	while(*dst++ = *src_++);
}

/*在字符串中查找目标字符出现的次数*/
uint32_t strchrs(const char* str_, uint8_t ch){
	ASSERT(str != NULL);
	uint32_t ch_cnt = 0;
	const char* str = str_;
	while(*str != 0){
		if(*str == ch)
			ch_cnt++;
		str++;
	}
	return ch_cnt;
}











