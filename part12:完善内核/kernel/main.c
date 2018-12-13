#include "init.h"
#include "print.h"
#include "debug.h"
#include "interrupt.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
static int pid = 0;

int main(void){
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    
	init_all();

	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");

	intr_enable();

	console_put_str("  main_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');
	thread_start("k_thread_a", 31, k_thread_a, "argA");
	thread_start("k_thread_b", 31, k_thread_b, "argB");
    
	while(1);
    return 0;
}

void k_thread_a(void* arg){
	char* para = arg;
   	console_put_str("kernel thread A pid is 0x");
	console_put_int(sys_getpid());
	console_put_char('\n');
	while(1);
}

void k_thread_b(void* arg){
	char* para = arg;
   	console_put_str("kernel thread B pid is 0x");
	console_put_int(sys_getpid());
	console_put_char('\n');
	while(1);
}

void u_prog_a(void){
	printf("user process A pid is 0x%x\n", getpid());
	while(1);
}

void u_prog_b(void){
	printf("user process B pid is 0x%x\n", getpid());
	while(1);	
}