#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"

//初始化信号量
void sema_init(struct semaphore* sema, uint8_t value){
	sema->value = value;
	list_init(&(sema->waiters));
}

//初始化锁
void lock_init(struct lock* lock){
	lock->holder = NULL;
	lock->holder_repeat_nr = 0;
	sema_init(&(lock->sema), 1);
}

//信号量dowm操作
void sema_down(struct semaphore* sema){
	enum intr_status old_status = intr_disable();
	while(sema->value == 0){
		ASSERT(!elem_find(&sema->waiters, &(get_thread_ptr()->general_tag)));
		if(elem_find(&sema->waiters, &get_thread_ptr()->general_tag)){
			PANIC("sema_down: thread blocked has been in waiters_list\n");		
		}
		//把当前线程加入到信号量的等待队列中
		list_append(&(sema->waiters), &(get_thread_ptr()->general_tag));
		thread_block(TASK_BLOCKED);
	}
	sema->value--;
	ASSERT(sema->value == 0);
	intr_set_status(old_status);
}

//信号量up操作
void sema_up(struct semaphore* sema){
	enum intr_status old_status = intr_disable();
	ASSERT(sema->value == 0);
	//判断信号量的等待队列中是否有元素，有的话把等待队列首元素解除阻塞
	if(!list_empty(&sema->waiters)){
		struct task_struct* thread_blocked = elemtoentry(struct task_struct, general_tag, list_pop(&sema->waiters));
		thread_unblock(thread_blocked);
	}	
	sema->value++;
	ASSERT(sema->value == 1);
	intr_set_status(old_status);
}

//获取锁
void lock_acquire(struct lock* plock){
	if(plock->holder != get_thread_ptr()){
		sema_down(&plock->sema);
		plock->holder = get_thread_ptr();
		ASSERT(plock->holder_repeat_nr == 0);
		plock->holder_repeat_nr = 1;	
	}
	else{
		plock->holder_repeat_nr++;
	}
}

//释放锁
void lock_release(struct lock* plock){
	ASSERT(plock->holder == get_thread_ptr());
	//若存在线程嵌套持有锁
	if(plock->holder_repeat_nr > 1){
		plock->holder_repeat_nr--;
		return;
	}
	ASSERT(plock->holder_repeat_nr == 1);
	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_up(&plock->sema);
}








