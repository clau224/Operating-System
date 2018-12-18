#include "timer.h"
#include "io.h"
#include "print.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY	   100
#define INPUT_FREQUENCY	   1193180
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40
#define COUNTER0_NO	   0
#define COUNTER_MODE	   2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

//记录自内核开启以来，总共的滴答数
uint32_t ticks;

static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
	//先往控制寄存器中写入控制字
	outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
	//然后往计数器中一次写入初始值的低八位和高八位
	outb(counter_port, (uint8_t)counter_value);
	outb(counter_port, (uint8_t)counter_value >> 8);
}

static void intr_timer_handler(void){
	struct task_struct* cur_thread = get_thread_ptr();
	ASSERT(cur_thread->stack_magic == 0x19970224);

	cur_thread->elapsed_ticks++;
	ticks++;
	
	if(cur_thread->ticks == 0)
		schedule();
	else
		cur_thread->ticks--;
	return;
}

void timer_init(void){
	put_str("timer_init start\n");
	frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
	register_handler(0x20, intr_timer_handler);
	put_str("timer_init done\n");
}

static void ticks_to_sleep(uint32_t sleep_ticks){
	uint32_t start_tick = ticks;
	while(ticks - start_tick < sleep_ticks){
		thread_yield();
	}
}

void mtime_sleep(uint32_t m_second){
	uint32_t sleep_ticks = DIV_ROUND_UP(m_second, 1000/IRQ0_FREQUENCY);
	ASSERT(sleep_ticks > 0);
	ticks_to_sleep(sleep_ticks);
}


