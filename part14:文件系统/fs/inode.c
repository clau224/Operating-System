#include "inode.h"
#include "fs.h"
#include "file.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "list.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

/* 用来存储inode位置 */
struct inode_position {
   bool	 two_sec;
   uint32_t sec_lba;
   uint32_t off_size;
};

/* 获取inode所在的扇区和扇区内的偏移量 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
   ASSERT(inode_no < 4096);
   uint32_t inode_table_lba = part->sb->inode_table_lba;

   uint32_t inode_size = sizeof(struct inode);
   //第inode_no号I结点相对于inode_table_lba的字节偏移量
   uint32_t off_size = inode_no * inode_size;
   //第inode_no号I结点相对于inode_table_lba的扇区偏移量	
   uint32_t off_sec  = off_size / 512;		  
   //待查找的inode所在扇区中的起始地址 
   uint32_t off_size_in_sec = off_size % 512;

   uint32_t left_in_sec = 512 - off_size_in_sec;
   //剩余空间比inode空间小，放不下
   if (left_in_sec < inode_size ) {
      inode_pos->two_sec = true;
   }
   //否则,所查找的inode未跨扇区
   else{	
      inode_pos->two_sec = false;
   }
   inode_pos->sec_lba = inode_table_lba + off_sec;
   inode_pos->off_size = off_size_in_sec;
}

/* 将inode写入到分区part */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {	 // io_buf是用于硬盘io的缓冲区
   uint8_t inode_no = inode->i_no;
   struct inode_position inode_pos;
   //inode位置信息会存入inode_pos
   inode_locate(part, inode_no, &inode_pos);
   ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
   
   struct inode pure_inode;
   memcpy(&pure_inode, inode, sizeof(struct inode));

   //以下三项只在内存中有作用，写回硬盘清除一下
   pure_inode.i_open_cnts = 0;
   pure_inode.write_deny = false;
   pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

   char* inode_buf = (char*)io_buf;
   //如果跨了两个扇区
   if (inode_pos.two_sec){
      ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
      //将待写入的inode拼入到这2个扇区中的相应位置
      memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
      //将拼接好的数据再写入磁盘
      ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
   }
   else{
      ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
      memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
      ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
   }
}

/* 根据i结点号返回相应的i结点 */
struct inode* inode_open(struct partition* part, uint32_t inode_no){
   struct list_elem* elem = part->open_inodes.head.next;
   struct inode* inode_found;
   while(elem != &part->open_inodes.tail){
      inode_found = elemtoentry(struct inode, inode_tag, elem);
      //找到了
      if(inode_found->i_no == inode_no) {
           inode_found->i_open_cnts++;
           return inode_found;
      }
      elem = elem->next;
   }

   struct inode_position inode_pos;
   //inode所在扇区地址和扇区内的字节偏移量会存在inode_pos中
   inode_locate(part, inode_no, &inode_pos);

   struct task_struct* cur = get_thread_ptr();
   uint32_t* cur_pagedir_bak = cur->pgdir;
   //将页表置空是为了被当做内核线程，在内核内存池分配空间
   cur->pgdir = NULL;
   inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
   cur->pgdir = cur_pagedir_bak;

   char* inode_buf;
   //如果跨扇区
   if (inode_pos.two_sec) {
      inode_buf = (char*)sys_malloc(1024);
      ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
   }
   else{
      inode_buf = (char*)sys_malloc(512);
      ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
   }
   memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

   list_push(&part->open_inodes, &inode_found->inode_tag);
   inode_found->i_open_cnts = 1;

   sys_free(inode_buf);
   return inode_found;
}

/* 关闭inode或减少inode的打开数 */
void inode_close(struct inode* inode){
   enum intr_status old_status = intr_disable();
   //说明此时没有任何进程打开此文件了
   if (--inode->i_open_cnts == 0) {
      //将该inode从part->open_inodes中去掉
      list_remove(&inode->inode_tag);
      struct task_struct* cur = get_thread_ptr();
      uint32_t* cur_pagedir_bak = cur->pgdir;
      cur->pgdir = NULL;
      sys_free(inode);
      cur->pgdir = cur_pagedir_bak;
   }
   intr_set_status(old_status);
}

/* 初始化一个inode*/
void inode_init(uint32_t inode_no, struct inode* new_inode) {
   new_inode->i_no = inode_no;
   new_inode->i_size = 0;
   new_inode->i_open_cnts = 0;
   new_inode->write_deny = false;

   /* 初始化块索引数组i_sector */
   uint8_t sec_idx = 0;
   while (sec_idx < 13) {
   /* i_sectors[12]为一级间接块地址 */
      new_inode->i_sectors[sec_idx] = 0;
      sec_idx++;
   }
}


/* 回收inode的数据块和inode本身 */
void inode_release(struct partition* part, uint32_t inode_no) {
   struct inode* inode_to_del = inode_open(part, inode_no);
   ASSERT(inode_to_del->i_no == inode_no);

   uint8_t block_idx = 0, block_cnt = 12;
   uint32_t block_bitmap_idx;
   uint32_t all_blocks[140] = {0}; 

   //下面要做的是把直接块和一级块(若存在)的地址放到all_blocks中
   while(block_idx < 12){
      all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
      block_idx++;
   }

   if (inode_to_del->i_sectors[12] != 0) {
      ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
      block_cnt = 140;

      //因为一级块表不在all_blocks中，所以单独回收
      block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
      ASSERT(block_bitmap_idx > 0);
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
   }
   
   //inode所有的块地址已经收集到all_blocks中,下面逐个回收
   block_idx = 0;
   while (block_idx < block_cnt) {
      if (all_blocks[block_idx] != 0) {
         block_bitmap_idx = 0;
         block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
         ASSERT(block_bitmap_idx > 0);
         bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
         bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      }
      block_idx++; 
   }

   //回收完block_bitmap中被删除文件的bit位，接着回收inode_bitmap中的bit位
   bitmap_set(&part->inode_bitmap, inode_no, 0);  
   bitmap_sync(cur_part, inode_no, INODE_BITMAP);
    
   inode_close(inode_to_del);
}
