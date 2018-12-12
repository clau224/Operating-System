#include "init.h"
#include "print.h"
#include "debug.h"
#include "interrupt.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "process.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

struct lock console_lock;

int test_var_a = 0, test_var_b = 0;

int main(void){
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    
	init_all();

	lock_init(&console_lock);

	thread_start("k_thread_a", 7, k_thread_a, "argA");
	thread_start("k_thread_b", 7, k_thread_b, "argB");
	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");

	intr_enable();
    
	while(1);
    return 0;
}

void k_thread_a(void* arg){
	char* para = arg;
   	while(1){
		lock_acquire(&console_lock);
		put_str("I am thread A, now test_car_a is  ");
		put_int(test_var_a);
		put_char('\n');
		lock_release(&console_lock);
   	}
}

void k_thread_b(void* arg){
	char* para = arg;
   	while(1){
		lock_acquire(&console_lock);
		put_str("*I am thread B, now test_car_b is  ");
		put_int(test_var_b);
		put_char('\n');
		lock_release(&console_lock);
   	}
}

void u_prog_a(void){
	while(1){
		test_var_a++;
	}
}

void u_prog_b(void){
	while(1){
		test_var_b++;
	}	
}
