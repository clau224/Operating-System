#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "stdint.h"

#define va_start(ap, v) ap = (va_list)&v 
#define va_arg(ap, t) *((t*)(ap += 4))
#define va_end(ap) ap = NULL

typedef char* va_list;
uint32_t printf(const char* str, ...);
uint32_t vsprintf(char* str, const char* format, va_list ap);
uint32_t sprintf(char* buf, const char* format, ...);

#endif
