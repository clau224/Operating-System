#include "stdint.h"
#include "global.h"
#include "thread.h"
#include "string.h"
#include "memory.h"

#define PG_SIZE 4096

static void kernel_thread(thread_func* function, void* func_arg){
	function(func_arg);
}

//在主函数中调用的是该函数，设置线程名称，优先级，线程执行函数和该函数参数
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
	struct task_struct* thread = get_kernel_pages(1);
	
	init_thread(thread, name, prio);
	
	thread_create(thread, function, func_arg);

	asm volatile ("movl %0, %%esp; \
					pop %%ebp; \
					pop %%ebx; \
					pop %%edi; \
					pop %%esi; \
					ret" : : "g" (thread->self_kstack) : "memory");
	return thread;
}


void init_thread(struct task_struct* pthread, char* name, int prio){
	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);
	pthread->status = TASK_RUNNING;
	pthread->priority = prio;
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->stack_magic = 0x19870916;
}


void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
	pthread->self_kstack -= sizeof(struct intr_stack);
	pthread->self_kstack -= sizeof(struct thread_stack);

	struct thread_stack* s = (struct thread_stack*)pthread->self_kstack;
	s->eip = kernel_thread;
	s->function = function;
	s->func_arg = func_arg;
	s->ebp = s->ebx = s->esi = s->edi = 0;
}





