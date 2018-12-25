#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "sync.h"
#include "list.h"
#include "bitmap.h"

/* 分区结构 */
struct partition {
   //起始扇区
   uint32_t start_lba;
   //扇区数
   uint32_t sec_cnt;
   //属于哪个硬盘
   struct disk* my_disk;

   struct list_elem part_tag;
   //该分区名称
   char name[8];
   //超级块
   struct super_block* sb;
   //块位图
   struct bitmap block_bitmap;
   //i节点位图
   struct bitmap inode_bitmap;
   //本分区打开的i节点队列
   struct list open_inodes;
};

/* 硬盘结构 */
struct disk {
   //硬盘名称
   char name[8];
   //硬盘归属于哪个通道
   struct ide_channel* my_channel;
   //标记本硬盘是主盘还是从盘
   uint8_t dev_no;
   //主分区
   struct partition prim_parts[4];
   //逻辑分区
   struct partition logic_parts[8];
};

/* ata通道结构 */
struct ide_channel {
   //通道名称
   char name[8];
   //通道起始端口号
   uint16_t port_base;
   //所用中断号
   uint8_t irq_no;
   //通道锁
   struct lock lock;	
   //是否等待中断
   bool expecting_intr;	
   //信号量
   struct semaphore disk_done;
   //通道上的两个硬盘，主盘和从盘
   struct disk devices[2];	
};

void ide_init(void);
extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
#endif