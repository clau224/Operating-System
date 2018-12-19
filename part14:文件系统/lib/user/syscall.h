#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

enum SYSCALL_NR {
   SYS_GETPID,
   SYS_WRITE,
   SYS_MALLOC,
   SYS_FREE,
   SYS_GETTNAME
};

uint32_t getpid(void);
uint32_t write(char* str);
char* gettname(void);
void* malloc(uint32_t);
void free(void*);

#endif

