#include "stdint.h"
#include "global.h"
#include "thread.h"
#include "string.h"
#include "memory.h"
#include "print.h"
#include "debug.h"
#include "interrupt.h"
#include "process.h"
#include "sync.h"

#define PG_SIZE 4096


//主线程PCB
struct task_struct* main_thread;
//idle线程
struct task_struct* idle_thread;
//就绪任务队列
struct list thread_ready_list;
//所有任务队列
struct list thread_all_list;
//一个保存线程节点的全局变量
static struct list_elem* thread_tag;

struct lock pid_lock;

extern void switch_to(struct task_struct* cur, struct task_struct* next);


//get pid
static pid_t allocate_pid(void){
	static pid_t next_pid = 0;
	lock_acquire(&pid_lock);
	next_pid++;
	lock_release(&pid_lock);
	return next_pid;
}

//获取当前PCB地址
struct task_struct* get_thread_ptr(){
	uint32_t esp;
	asm ("mov %%esp, %0" : "=g" (esp));
	return (struct task_struct*)(esp & 0xfffff000);
}

//由kernel_thread来执行functional
static void kernel_thread(thread_func* function, void* func_arg){
	intr_enable();
	function(func_arg);
}

//新建线程调用的是该函数，设置线程名称，优先级，线程执行函数和该函数参数
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
	struct task_struct* thread = get_kernel_pages(1);
	
	init_thread(thread, name, prio);
	
	thread_create(thread, function, func_arg);

	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	list_append(&thread_ready_list, &thread->general_tag);

	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	list_append(&thread_all_list, &thread->all_list_tag);	

	return thread;
}


void init_thread(struct task_struct* pthread, char* name, int prio){
	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);
	pthread->pid = allocate_pid();
	
	if(pthread == main_thread)
		pthread->status = TASK_RUNNING;
	else
		pthread->status = TASK_READY;
	pthread->priority = prio;
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->stack_magic = 0x19970224;

	pthread->fd_table[0] = 0;
   	pthread->fd_table[1] = 1;
   	pthread->fd_table[2] = 2;

   	/* 其余的全置为-1 */
   	uint8_t fd_idx = 3;
   	while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
      	pthread->fd_table[fd_idx] = -1;
      	fd_idx++;
   	}
}


void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
	//在页表的顶端让出intr_stack和thread_stack的空间
	pthread->self_kstack -= sizeof(struct intr_stack);
	pthread->self_kstack -= sizeof(struct thread_stack);

	struct thread_stack* s = (struct thread_stack*)pthread->self_kstack;
	s->eip = kernel_thread;
	s->function = function;
	s->func_arg = func_arg;
	s->ebp = s->ebx = s->esi = s->edi = 0;
}

static void make_main_thread(void){
	main_thread = get_thread_ptr();
	init_thread(main_thread, "main", 31);
	
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

static void idle(void* arg UNUSED){
	while(1){
		thread_block(TASK_BLOCKED);
		asm volatile("sti; hlt" : : : "memory");
	}
}


void thread_yield(){
	struct task_struct* cur = get_thread_ptr();
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}


//进行线程调度
void schedule(){
	ASSERT(intr_get_status() == INTR_OFF);
	
	struct task_struct* cur = get_thread_ptr();
	if(cur->status == TASK_RUNNING){
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;
		cur->status = TASK_READY;
	}
	else{
	}
	if(list_empty(&thread_ready_list)){
		thread_unblock(idle_thread);
	}
	thread_tag = NULL;
	thread_tag = list_pop(&thread_ready_list);
	//我们由thread_tag来推得其对应的task_struct
	struct task_struct* next = elemtoentry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;
	process_activate(next);
	switch_to(cur, next);
}

//这个是在init中调用的函数
void thread_init(void){
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock);
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread init done\n");
}

//将线程设置为阻塞，调用者是希望被阻塞的线程
void thread_block(enum task_status stat){
	ASSERT((stat == TASK_BLOCKED || stat == TASK_WAITING || stat == TASK_HANGING));
	enum intr_status old_status = intr_disable();
	struct task_struct* cur = get_thread_ptr();
	cur->status = stat;
	schedule();
	intr_set_status(old_status);
}

//将线程解除阻塞，调用者是执行V操作的线程
void thread_unblock(struct task_struct* pthread){
	enum intr_status old_status = intr_disable();
	ASSERT((pthread->status == TASK_BLOCKED || pthread->status == TASK_WAITING || pthread->status == TASK_HANGING));
	if(pthread->status != TASK_READY){
		ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
		list_push(&thread_ready_list, &pthread->general_tag);
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}




