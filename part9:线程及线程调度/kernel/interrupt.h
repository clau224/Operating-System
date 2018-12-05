#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);

//中断状态枚举
enum intr_status{
	INTR_OFF,
	INTR_ON
};

//获取当前中断状态
enum intr_status intr_get_status();
//将中断状态设置为开中断，返回值是之前的状态
enum intr_status intr_enable();
//设置为关中断
enum intr_status intr_disable();
//将中断状态设置为status
enum intr_status intr_set_status(enum intr_status status);


#endif
