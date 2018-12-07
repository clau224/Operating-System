#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"

typedef void thread_func(void*);

//进程的6个状态
enum task_status{
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITING,
	TASK_HANGING,
	TASK_DIED
};

//发生中断时保护的上下文
struct intr_stack{
	uint32_t vec_no;		//这个是中断号
	uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;		
	uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
	//低特权级到高特权级会将以下信息压入
	uint32_t err_code;		 
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

struct thread_stack {
   	uint32_t ebp;
   	uint32_t ebx;
   	uint32_t edi;
   	uint32_t esi;

	void (*eip) (thread_func* func, void* func_arg);

	void (*unused_retaddr);
   	thread_func* function; 
   	void* func_arg;
};

struct task_struct{
	//定义的是PCB中的栈顶指针
	uint32_t* self_kstack;
	//进程状态
	enum task_status status;
	//线程的名字
	char name[16];
	//优先级
	uint8_t priority;

	//每次在处理器上执行的时间(滴答)数，这个可以反映优先级
	uint8_t ticks;
	//已经执行过的滴答数
	uint32_t elapsed_ticks;

	//作为在等待队列中的标记
	struct list_elem general_tag;
	//作为在全部线程队列中的标记
	struct list_elem all_list_tag;

	//本进程页表的虚拟地址，若本结构体是线程的，则为NULL
	uint32_t* pgdir;

	//自己定义的魔数，目测作者设置的是自己生日～～
	uint32_t stack_magic;
};


void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);

void init_thread(struct task_struct* pthread, char* name, int prio);

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);

void thread_init(void);

struct task_struct* get_thread_ptr();

void schedule();


#endif

	


