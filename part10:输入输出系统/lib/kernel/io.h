#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

//向端口port中写入一个字节
static inline void outb(uint16_t port, uint8_t data){
	asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

//将addr为起始的word_cnt个字符输入到port中
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt){
	asm volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}

//从端口port中读出一个字节并返回
static inline uint8_t inb(uint16_t port){
	uint8_t data;
	asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
	return data;
}

//从端口port中读出word_cnt个字符，并写入到addr上
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt){
	asm volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}

#endif


