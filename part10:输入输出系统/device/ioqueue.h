#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

struct ioqueue{
	//涉及到互斥，所以成员有有一个锁
	struct lock lock;
	//生产者队列
	struct task_struct* producer;
	//消费者队列
	struct task_struct* consumer;
	//缓冲区
	char buf[bufsize];
	//队头和队尾
	int32_t head;
	int32_t tail;
};

//初始化io队列
void ioqueue_init(struct ioqueue* ioq);
//判断队列是否已满
bool ioq_full(struct ioqueue* ioq);
//判断队列是否为空
bool ioq_empty(struct ioqueue* ioq);
//唤醒waiter
static void wakeup(struct task_struct** waiter);
//消费者从ioq队列中获取字符
char ioq_getchar(struct ioqueue* ioq);
//生产者向ioq队列中写入字符
void ioq_putchar(struct ioqueue* ioq, char byte);


#endif
