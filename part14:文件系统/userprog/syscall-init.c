#include "syscall-init.h"
#include "syscall.h"
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "sync.h"

#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid(void){
	return get_thread_ptr()->pid;
}

uint32_t sys_gettname(void){
	return (uint32_t)get_thread_ptr()->name;
}

void syscall_init(void){
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	syscall_table[SYS_READ] = sys_read;
	syscall_table[SYS_GETTNAME] = sys_gettname;
	syscall_table[SYS_MALLOC] = sys_malloc;
	syscall_table[SYS_FREE] = sys_free;
	syscall_table[SYS_LOCK_ACQUIRE] = lock_acquire;
	syscall_table[SYS_LOCK_RELEASE] = lock_release;
	syscall_table[SYS_LSEEK] = sys_lseek;

	put_str("syscall_init done\n");
}