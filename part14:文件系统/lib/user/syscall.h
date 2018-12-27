#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"
#include "sync.h"

enum SYSCALL_NR {
   SYS_GETPID,
   SYS_WRITE,
   SYS_READ,
   SYS_MALLOC,
   SYS_FREE,
   SYS_GETTNAME,
   SYS_LOCK_ACQUIRE,
   SYS_LOCK_RELEASE
};

uint32_t getpid(void);
char* gettname(void);
void* malloc(uint32_t);
void free(void*);
uint32_t write(int32_t fd, const void* buf, uint32_t count);
void lock(struct lock* lck);
void unlock(struct lock* lck);

#endif

