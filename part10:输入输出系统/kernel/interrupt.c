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


//eflags寄存器中，if处于第23位
#define EFLAGS_IF 0x00000200


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
};

//中断门描述符数组
static struct gate_desc idt[IDT_DESC_CNT];
//定义在kernel.S中的存储中断入口程序地址的数组
extern intr_handler intr_entry_table[IDT_DESC_CNT];
//保存异常的名字的数组
char* intr_name[IDT_DESC_CNT];
//中断处理程序数组，数组中存放的是中断处理程序
intr_handler idt_table[IDT_DESC_CNT];



//初始化中断控制器8259A
static void pic_init(void){
	//初始化主片
	outb (PIC_M_CTRL, 0x11);
	outb (PIC_M_DATA, 0x20);
	outb (PIC_M_DATA, 0x04); 
	outb (PIC_M_DATA, 0x01);
	//初始化从片
	outb (PIC_S_CTRL, 0x11);
	outb (PIC_S_DATA, 0x28);
	outb (PIC_S_DATA, 0x02);
	outb (PIC_S_DATA, 0x01);
	//打开主片上IR0,也就是目前只接受时钟产生的中断
	outb (PIC_M_DATA, 0xfe);
	outb (PIC_S_DATA, 0xff);

	put_str("   pic_init done\n");
}



//创建中断描述符
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function){
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16 ;
}

//初始化中断描述符表
static void idt_desc_init(void){
	int i;
	for(i=0; i<IDT_DESC_CNT; i++){
		make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	}
	put_str("   idt_desc_init done\n");
}

//通用的中断处理函数
static void general_intr_handler(uint8_t vec_nr){
	if(vec_nr == 0x27 || vec_nr == 0x2f)
		return;

	set_cursor(0);
	int cursor_pos = 0;
	while(cursor_pos < 320){
		put_char(' ');
		cursor_pos++;
	}	
	set_cursor(0);
	put_str(" !!! excetion message begin !!! \n");	
	set_cursor(84);
	put_str(intr_name[vec_nr]);
	if(vec_nr == 14){
		int page_fault_vaddr = 0;
		asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));
		put_str("\n page fault addr is ");
		put_int(page_fault_vaddr);
	}
	put_str("\n !!! excetion message end !!! \n");
	while(1);
}

//完成中断处理函数注册及异常名称注册
static void exception_init(void){
	int i;
	for(i=0; i<IDT_DESC_CNT; i++){
		idt_table[i] = general_intr_handler;
		intr_name[i] = "unknown";
	}
	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "NMI Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF Overflow Exception";
   	intr_name[5] = "#BR BOUND Range Exceeded Exception";
  	intr_name[6] = "#UD Invalid Opcode Exception";
   	intr_name[7] = "#NM Device Not Available Exception";
   	intr_name[8] = "#DF Double Fault Exception";
   	intr_name[9] = "Coprocessor Segment Overrun";
   	intr_name[10] = "#TS Invalid TSS Exception";
   	intr_name[11] = "#NP Segment Not Present";
   	intr_name[12] = "#SS Stack Fault Exception";
   	intr_name[13] = "#GP General Protection Exception";
   	intr_name[14] = "#PF Page-Fault Exception";
   	// intr_name[15] 第15项是intel保留项，未使用
   	intr_name[16] = "#MF x87 FPU Floating-Point Error";
   	intr_name[17] = "#AC Alignment Check Exception";
   	intr_name[18] = "#MC Machine-Check Exception";
   	intr_name[19] = "#XF SIMD Floating-Point Exception";
}

//中断处理程序数组中，第vector_no个元素处注册安装中断处理程序function
void register_handler(uint8_t vector_no, intr_handler function){
	idt_table[vector_no] = function;	
}


//完成有关中断的所有初始化工作
void idt_init(void){
	put_str("idt_init start\n");
	idt_desc_init();
	exception_init();
	pic_init();
	uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
   	asm volatile("lidt %0" : : "m" (idt_operand));
   	put_str("idt_init done\n");
}


//获取当前中断状态
enum intr_status intr_get_status(){
	uint32_t eflags = 0;
	asm volatile("pushfl; popl %0" : "=g" (eflags));
	return EFLAGS_IF & eflags ? INTR_ON : INTR_OFF;
}

enum intr_status intr_enable(){
	enum intr_status old_status;
	old_status = intr_get_status();
	if(old_status == INTR_OFF){
		//sti指令将IF位置1
		asm volatile("sti");
	}
	return old_status;
}

enum intr_status intr_disable(){
	enum intr_status old_status;
	old_status = intr_get_status();
	if(old_status == INTR_ON){
		//cli指令将IF位置0，关中断
		asm volatile("cli" : : : "memory");	
	}
	return old_status;
}

enum intr_status intr_set_status(enum intr_status status){
	return status & INTR_ON ? intr_enable() : intr_disable();
}





















