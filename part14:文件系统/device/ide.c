#include "ide.h"
#include "sync.h"
#include "io.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "console.h"
#include "timer.h"
#include "string.h"
#include "list.h"

//data端口
#define reg_data(channel)	 (channel->port_base + 0)
//读操作时的error端口
#define reg_error(channel)	 (channel->port_base + 1)
//制定待读取扇区数的端口
#define reg_sect_cnt(channel)	 (channel->port_base + 2)
//存储扇区地址低八位的端口
#define reg_lba_l(channel)	 (channel->port_base + 3)
//中八位
#define reg_lba_m(channel)	 (channel->port_base + 4)
//高八位(LBA28一共28bit，这是17-24)
#define reg_lba_h(channel)	 (channel->port_base + 5)
//device，其中低四位(0-3)是LBA最高四位
//第4位用来指定通道上的主盘从盘，第6位决定是否启用LBA
#define reg_dev(channel)	 (channel->port_base + 6)
//第0位error,表示命令是否出错，第3位表示数据是否准备好
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
//控制端口
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

//硬盘忙
#define BIT_STAT_BSY	 0x1<<7
//驱动器准备好	
#define BIT_STAT_DRDY	 0x1<<6  
//数据传输准备好     
#define BIT_STAT_DRQ	 0x1<<3

#define BIT_DEV_MBS	0xa0
#define BIT_DEV_LBA	0x40
#define BIT_DEV_DEV	0x10

//identify指令
#define CMD_IDENTIFY	   0xec
//读扇区指令
#define CMD_READ_SECTOR	   0x20
//写扇区指令
#define CMD_WRITE_SECTOR   0x30
//最大扇区数，80M的硬盘
#define max_lba ((80*1024*1024/512) - 1)	

//通道数
uint8_t channel_cnt;
//两个ide通道
struct ide_channel channels[2];
//总扩展分区的起始lba
int32_t ext_lba_base = 0;
//硬盘主分区和逻辑分区的下表
uint8_t p_no = 0, l_no = 0;
//分区队列
struct list partition_list;

//区表项的结构体
struct partition_table_entry{
	//是否可引导
	uint8_t  bootable;
	//以下三项分别为起始的磁头号、起始扇区和起始柱面
   	uint8_t  start_head;
   	uint8_t  start_sec;
   	uint8_t  start_chs;
   	//分区类型
   	uint8_t  fs_type;
   	//结束磁头号，结束扇区，结束柱面
   	uint8_t  end_head;
   	uint8_t  end_sec;
   	uint8_t  end_chs;
   	//本分区起始扇区的lba地址
   	uint32_t start_lba;
   	//本分区的扇区数目
   	uint32_t sec_cnt;
   	//设置结构体内部不对齐，这样控制结构体的大小为所有成员大小之和
} __attribute__ ((packed));

//引导扇区,mbr或ebr所在的扇区
struct boot_sector {
	//代码部分
   	uint8_t  other[446];
   	//分区表项，长度为16字节*4
   	struct   partition_table_entry partition_table[4];
   //结束标志0x55,0xaa
   uint16_t signature;
} __attribute__ ((packed));


//向device端口写入
static void select_disk(struct disk* hd) {
   	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
   	if (hd->dev_no == 1) {
      	reg_device |= BIT_DEV_DEV;
   	}
   	outb(reg_dev(hd->my_channel), reg_device);
}

//写入起始扇区地址lba和要读写的扇区数sec_cnt
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
   	ASSERT(lba <= max_lba);
   	struct ide_channel* channel = hd->my_channel;

   	outb(reg_sect_cnt(channel), sec_cnt);

   	outb(reg_lba_l(channel), lba);
   	outb(reg_lba_m(channel), lba >> 8);
   	outb(reg_lba_h(channel), lba >> 16);

   	//重新写入device端口，因为要写lba最高四位
   	outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

//向通道channel发命令cmd
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
	//只要向硬盘发出了命令便将此标记置为true,硬盘中断处理程序需要根据它来判断
   	channel->expecting_intr = true;
   	outb(reg_cmd(channel), cmd);
}

//硬盘读入sec_cnt个扇区的数据到buf
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
   	uint32_t size_in_byte;
   	if (sec_cnt == 0) {
      	size_in_byte = 256 * 512;
   	}
   	else { 
      	size_in_byte = sec_cnt * 512; 
   	}
   	insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

//将buf中sec_cnt扇区的数据写入硬盘
static void write_to_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
   	uint32_t size_in_byte;
   	if (sec_cnt == 0) {
      	size_in_byte = 256 * 512;
   	} 
   	else { 
      	size_in_byte = sec_cnt * 512; 
   	}
   	outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

//等待30秒，等待数据传输完成，未完成返回false
static bool busy_wait(struct disk* hd) {
   	struct ide_channel* channel = hd->my_channel;
   	uint16_t time_limit = 30 * 1000;
   	while (time_limit -= 10 >= 0) {
      	if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
	 		return (inb(reg_status(channel)) & BIT_STAT_DRQ);
      	} 
      	else{
	 		mtime_sleep(10);
      	}
   	}
   	return false;
}

//从硬盘读取sec_cnt个扇区到buf
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){   // 此处的sec_cnt为32位大小
   	ASSERT(lba <= max_lba);
   	ASSERT(sec_cnt > 0);
   	lock_acquire (&hd->my_channel->lock);

   	select_disk(hd);

   	//本次要转移的扇区数
   	uint32_t secs_op;
   	//已读取完成的扇区数
   	uint32_t secs_done = 0;
   	while(secs_done < sec_cnt) {
      	if ((secs_done + 256) <= sec_cnt) {
	 		secs_op = 256;
      	}
      	else {
	 		secs_op = sec_cnt - secs_done;
      	}

   		//设置待读入的扇区数和起始扇区号
      	select_sector(hd, lba + secs_done, secs_op);

    	   //设置指令，读扇区
      	cmd_out(hd->my_channel, CMD_READ_SECTOR);

	   	//当磁盘在读写数据时，把自己阻塞
      	sema_down(&hd->my_channel->disk_done);

      	//等最长30秒，检查硬盘是否准备好
      	if (!busy_wait(hd)) {
	 		char error[64];
	 		sprintf(error, "%s read sector %d failed!!!!!!\n", hd->name, lba);
	 		PANIC(error);
      	}

   		//从扇区里转移数据
      	read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
      	secs_done += secs_op;
   	}
   	lock_release(&hd->my_channel->lock);
}

//将buf中sec_cnt扇区数据写入硬盘
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt){
   	ASSERT(lba <= max_lba);
   	ASSERT(sec_cnt > 0);
   	lock_acquire (&hd->my_channel->lock);

   	select_disk(hd);

   	uint32_t secs_op;
   	uint32_t secs_done = 0;
   	while(secs_done < sec_cnt) {
      	if((secs_done + 256) <= sec_cnt) {
	 		secs_op = 256;
      	} 
      	else{
	 		secs_op = sec_cnt - secs_done;
      	}

   
      	select_sector(hd, lba + secs_done, secs_op);

      	cmd_out(hd->my_channel, CMD_WRITE_SECTOR);	      // 准备开始写数据

      	if (!busy_wait(hd)) {			      // 若失败
	 		char error[64];
	 		sprintf(error, "%s write sector %d failed!!!!!!\n", hd->name, lba);
	 		PANIC(error);
      	}


      	write_to_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

      
      	sema_down(&hd->my_channel->disk_done);
      	secs_done += secs_op;
   	}
   	lock_release(&hd->my_channel->lock);
}

//硬盘中断处理程序
void intr_hd_handler(uint8_t irq_no) {
   	ASSERT(irq_no == 0x2e || irq_no == 0x2f);
   	uint8_t ch_no = irq_no - 0x2e;
   	struct ide_channel* channel = &channels[ch_no];
   	ASSERT(channel->irq_no == irq_no);
   	if (channel->expecting_intr) {
      	channel->expecting_intr = false;
      	sema_up(&channel->disk_done);
		//读取状态寄存器使硬盘控制器认为此次的中断已被处理,
 		//从而硬盘可以继续执行新的读写
      	inb(reg_status(channel));
   }
}

//把dst中的字符两两交换顺序，然后存入到buf中
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len){
	uint8_t i;
	for(i = 0; i < len; i += 2){
		buf[i+1] = *dst;
		dst++;
		buf[i] = *dst;
		dst++;
	}
	buf[i] = '\0';
}

//获取硬盘参数信息
static void identify_disk(struct disk* hd){
	char id_info[512];
	select_disk(hd);
	cmd_out(hd->my_channel, CMD_IDENTIFY);

	sema_down(&hd->my_channel->disk_done);

	if(!busy_wait(hd)){
		char error[64];
      	sprintf(error, "%s identify failed!!!!!!\n", hd->name);
      	PANIC(error);
	}

	read_from_sector(hd, id_info, 1);

	char buf[64];
	uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
	swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
   	printk("   disk %s info:\n      SN: %s\n", hd->name, buf);
   	memset(buf, 0, sizeof(buf));
   	swap_pairs_bytes(&id_info[md_start], buf, md_len);
   	printk("      MODULE: %s\n", buf);
   	uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
   	printk("      SECTORS: %d\n", sectors);
   	printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

//扫描硬盘hd中地址为ext_lba的扇区中的所有分区
static void partition_scan(struct disk* hd, uint32_t ext_lba) {
   	struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
   	ide_read(hd, ext_lba, bs, 1);
   	struct partition_table_entry* p = bs->partition_table;
   	uint8_t i = 0;

   //深度优先遍历分区表，找到所有分区
   	while (i++ < 4){
   		//如果是扩展分区的话
      	if (p->fs_type == 0x5){
      		//总扩展分区的起始扇区号不为0，就说明之前已找到主引导所在扇区
	 		if (ext_lba_base != 0){
	    		partition_scan(hd, p->start_lba + ext_lba_base);
	 		}
	 		//ext_lba_base为0表示是第一次读取引导块,也就是主引导记录所在的扇区
	 		else{
	 			ext_lba_base = p->start_lba;
	 			partition_scan(hd, p->start_lba);
	 		}
      	} 
      	else if (p->fs_type != 0) { // 若是有效的分区类型
	 		if(ext_lba == 0) {	 // 此时全是主分区
	    		hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
	    		hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
	    		hd->prim_parts[p_no].my_disk = hd;
	    		list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
	    		sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
	    		p_no++;
	    		ASSERT(p_no < 4);	    // 0,1,2,3
	 		} 
	 		else {
	    		hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
	    		hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
	    		hd->logic_parts[l_no].my_disk = hd;
	    		list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
	    		sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);	 // 逻辑分区数字是从5开始,主分区是1～4.
	    		l_no++;
	   		 	if (l_no >= 8)    // 只支持8个逻辑分区,避免数组越界
	       			return;
	 		}
      	} 
      	p++;
   }
   sys_free(bs);
}

//打印分区信息
static bool partition_info(struct list_elem* pelem, int arg UNUSED) {
   struct partition* part = elemtoentry(struct partition, part_tag, pelem);
   printk("   %s start_lba:0x%x, sec_cnt:0x%x\n",part->name, part->start_lba, part->sec_cnt);
   return false;
}


/*硬盘初始化*/
void ide_init(){
	printk("ide_init start\n");
	uint8_t hd_cnt = *((uint8_t*)(0x475));
	ASSERT(hd_cnt>0);
	channel_cnt = DIV_ROUND_UP(hd_cnt, 2);

	struct ide_channel* channel;
	uint8_t i = 0;

	while (i < channel_cnt) {
		channel = &channels[i];
      	sprintf(channel->name, "ide%d", i);

      	//为每个ide通道初始化端口基址及中断向量
      	switch (i) {
	 		case 0:
	    		channel->port_base	 = 0x1f0;
	    		channel->irq_no	 = 0x20 + 14;
	    		break;
	 		case 1:
	    		channel->port_base	 = 0x170;
	    		channel->irq_no	 = 0x20 + 15;
	    		break;
      	}

      	channel->expecting_intr = false;
      	
      	lock_init(&channel->lock);		     

      	sema_init(&channel->disk_done, 0);

      	register_handler(channel->irq_no, intr_hd_handler);

      	uint8_t j = 0;
      	while (j < 2) {
	        struct disk* hd = &channel->devices[j];
	    	hd->my_channel = channel;
	    	hd->dev_no = j;
	       	sprintf(hd->name, "sd%c", 'a' + i * 2 + j);
	    	identify_disk(hd);
	      	if (j != 0) {
	      		//只检查hd80M.img硬盘上的分区
	          	partition_scan(hd, 0);
	       	}
	       	p_no = 0, l_no = 0;
	       	j++; 
      	}
       	j = 0;			  	   // 将硬盘驱动器号置0,为下一个channel的两个硬盘初始化。
       	
      	i++;
   	}
      printk("\n   all partition info\n");
      /* 打印所有分区信息 */
      list_traversal(&partition_list, partition_info, (int)NULL);

   	printk("ide_init done\n");
}
