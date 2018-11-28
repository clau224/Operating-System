#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

//总共支持的中断数
#define IDT_DESC_CNT 0x21
//主片的控制端口，0x20
#define PIC_M_CTRL 0x20
//主片的数据端口，0x21
#define PIC_M_DATA 0x21
//从片的控制端口，0xa0
#define PIC_S_CTRL 0xa0
//从片的数据端口，0xa1
#define PIC_S_DATA 0xa1

struct gate_desc{
	//中断处理程序在目标代码段的偏移量的15-0位
	uint16_t func_offset_low_word;
	//中断处理程序目标代码段描述符选择子
	uint16_t selector;
	//此项是固定值？
	uint8_t dcount;
	//记录存在位P，DPL，TYPE字段
	uint8_t attribute;
	//中断处理程序在目标段内的偏移量的31~16位
	uint16_t func_offset_high_word;
}

//中断门描述符数组
static struct gate_desc idt[IDT_DESC_CNT];
//定义在kernel.S中的存储中断入口程序地址的数组
extern intr_handler intr_entry_table[IDT_DESC_CNT];

//创建中断描述符
static  void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function){
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16 ;
}

//初始化中断描述符表
static void idt_desc_init(void){
	for(int i=0; i<IDT_DESC_CNT; i++){
		make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	}
	put_str("   idt_desc_init done\n");
}

//完成有关中断的所有初始化工作
void idt_init(){
	put_str("idt_init start\n");
	idt_desc_init();
	pic_init();
	uint64_t idt_operand = ((sizeof(idt) -1) | ((uint64_t)((uint32_t)idt << 16)));
	asm volatile("lidt %0" : : "m" (idt_operand));
	put_str("idt_init done\n");
}




