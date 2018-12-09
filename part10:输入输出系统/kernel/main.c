#include "init.h"
#include "print.h"
#include "debug.h"
#include "interrupt.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void){
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    
	init_all();

	thread_start("k_thread_a", 31, k_thread_a, "A said:");
	thread_start("k_thread_b", 8, k_thread_b, "B said:");

	intr_enable();
    
	while(1);
	/*
    while(1){
		console_put_str("Main ");
	}
	*/
    return 0;
}

void k_thread_a(void* arg){
   	while(1){
      	enum intr_status old_status = intr_disable();
		if(!ioq_empty(&buffer)){
			console_put_str(arg);
			char byte = ioq_getchar(&buffer);
			console_put_char(byte);
			console_put_char('\n');
		}
		intr_set_status(old_status);
   	}
}

void k_thread_b(void* arg){
	while(1){
      	enum intr_status old_status = intr_disable();
		if(!ioq_empty(&buffer)){
			console_put_str(arg);
			char byte = ioq_getchar(&buffer);
			console_put_char(byte);
			console_put_char('\n');
		}
		intr_set_status(old_status);
   	}
}
