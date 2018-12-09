#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

//初始化io队列
void ioqueue_init(struct ioqueue* ioq){
	lock_init(&ioq->lock);
	ioq->producer = ioq->consumer = NULL;
	ioq->head = ioq->tail = 0;
}

//返回pos在缓冲区的下一个位置
static int32_t next_pos(int32_t pos){
	return (pos+1)%bufsize;
}

//判断队列是否已满
bool ioq_full(struct ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	return next_pos(ioq->head) == ioq->tail;
}

//判断队列是否为空
bool ioq_empty(struct ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

//使当前生产者或消费者阻塞，等待资源或等待空间
//ioqueue结构体中，生产者消费者都是task_struct指针，所以这里参数用了二维指针来改变task_struct的值
static void ioq_wait(struct task_struct** waiter){
	ASSERT(*waiter == NULL && waiter != NULL);
	*waiter = get_thread_ptr();
	thread_block(TASK_BLOCKED);
}

//唤醒waiter
static void wakeup(struct task_struct** waiter){
	ASSERT(*waiter != NULL);
	thread_unblock(*waiter);
	*waiter = NULL;
}

//消费者从ioq队列中获取字符
char ioq_getchar(struct ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	while(ioq_empty(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	char byte = ioq->buf[ioq->tail];
	ioq->tail = next_pos(ioq->tail);
	if(ioq->producer != NULL)
		wakeup(&ioq->producer);
	return byte;
}

//生产者向ioq队列中写入字符
void ioq_putchar(struct ioqueue* ioq, char byte){
	ASSERT(intr_get_status() == INTR_OFF);
	while(ioq_full(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	ioq->buf[ioq->tail] = byte;
	ioq->head = next_pos(ioq->head);
	if(ioq->consumer != NULL)
		wakeup(&ioq->consumer);
}

