#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"
#include "thread.h"

//信号量结构体
struct semaphore{
	uint8_t value;
	struct list waiters;
};

//锁结构体
struct lock{
	//锁的持有者线程
	struct task_struct* holder;
	//信号量实现锁
	struct semaphore sema;
	//锁持有者重复申请锁的次数
	uint32_t holder_repeat_nr;
};

//初始化信号量
void sema_init(struct semaphore* sema, uint8_t value);
//初始化锁
void lock_init(struct lock* lock);

//信号量dowm操作
void sema_down(struct semaphore* sema);
//信号量up操作
void sema_up(struct semaphore* sema);
//获取锁
void lock_acquire(struct lock* plock);
//释放锁
void lock_release(struct lock* plock);

#endif
