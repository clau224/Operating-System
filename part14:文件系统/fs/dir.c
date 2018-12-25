#include "dir.h"
#include "stdint.h"
#include "inode.h"
#include "file.h"
#include "fs.h"
#include "stdio-kernel.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "super_block.h"

//根目录
struct dir root_dir;

/* 打开根目录 */
void open_root_dir(struct partition* part) {
   root_dir.inode = inode_open(part, part->sb->root_inode_no);
   root_dir.dir_pos = 0;
}

/* 在分区part上打开i结点为inode_no的目录并返回目录指针 */
struct dir* dir_open(struct partition* part, uint32_t inode_no){
   struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
   pdir->inode = inode_open(part, inode_no);
   pdir->dir_pos = 0;
   return pdir;
}

/* 在part分区内的pdir目录内寻找名为name的文件或目录,
 * 找到后返回true并将其目录项存入dir_e,否则返回false */
bool search_dir_entry(struct partition* part, struct dir* pdir, \
		     const char* name, struct dir_entry* dir_e) {
   uint32_t block_cnt = 140;	 // 12个直接块+128个一级间接块=140块

   //12个直接块大小+128个间接块,140个指针，共560字节
   uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);
   if (all_blocks == NULL){
      printk("search_dir_entry: sys_malloc for all_blocks failed");
      return false;
   }

   uint32_t block_idx = 0;
   while (block_idx < 12) {
      all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
      block_idx++;
   }
   block_idx = 0;

   //如果含有一级间接块
   if (pdir->inode->i_sectors[12] != 0) {
      ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
   }

   //buf用作缓冲区
   uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
   //p_de用于挨个检查buf中的目录项
   struct dir_entry* p_de = (struct dir_entry*)buf;	
   uint32_t dir_entry_size = part->sb->dir_entry_size;
   //1扇区内可容纳的目录项个数
   uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

   while(block_idx < block_cnt){		  
      //空则继续找
      if(all_blocks[block_idx] == 0){
	        block_idx++;
	        continue;
      }
      ide_read(part->my_disk, all_blocks[block_idx], buf, 1);

      uint32_t dir_entry_idx = 0;
      while (dir_entry_idx < dir_entry_cnt){
	     //若找到了
	     if (!strcmp(p_de->filename, name)){
	        memcpy(dir_e, p_de, dir_entry_size);
	        sys_free(buf);
	        sys_free(all_blocks);
	        return true;
	     }
	     dir_entry_idx++;
	     p_de++;
      }
      block_idx++;
      p_de = (struct dir_entry*)buf; 
      memset(buf, 0, SECTOR_SIZE);
   }
   sys_free(buf);
   sys_free(all_blocks);
   return false;
}

/* 关闭目录 */
void dir_close(struct dir* dir) {
   if (dir == &root_dir) {
   /* 不做任何处理直接返回*/
      return;
   }
   inode_close(dir->inode);
   sys_free(dir);
}

/* 在内存中初始化目录项p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de){
   ASSERT(strlen(filename) <=  MAX_FILE_NAME_LEN);

   /* 初始化目录项 */
   memcpy(p_de->filename, filename, strlen(filename));
   p_de->i_no = inode_no;
   p_de->f_type = file_type;
}

/* 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {
   struct inode* dir_inode = parent_dir->inode;
   uint32_t dir_size = dir_inode->i_size;
   uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

   ASSERT(dir_size % dir_entry_size == 0);	 // dir_size应该是dir_entry_size的整数倍

   uint32_t dir_entrys_per_sec = (512 / dir_entry_size);       // 每扇区最大的目录项数目
   int32_t block_lba = -1;

   /* 将该目录的所有扇区地址(12个直接块+ 128个间接块)存入all_blocks */
   uint8_t block_idx = 0;
   uint32_t all_blocks[140] = {0};	  // all_blocks保存目录所有的块

   /* 将12个直接块存入all_blocks */
   while (block_idx < 12) {
      all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
      block_idx++;
   }

   struct dir_entry* dir_e = (struct dir_entry*)io_buf;
   int32_t block_bitmap_idx = -1;

   /* 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,
    * 在不超过文件大小的情况下申请新扇区来存储新目录项 */
   block_idx = 0;
   while (block_idx < 140) { 
      block_bitmap_idx = -1;
      //在三种情况下分配块
      if (all_blocks[block_idx] == 0){
	        block_lba = block_bitmap_alloc(cur_part);
	        if (block_lba == -1) {
	           printk("alloc block bitmap for sync_dir_entry failed\n");
	           return false;
	        }

	        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	        ASSERT(block_bitmap_idx != -1);
            //每分配一个块就同步一次block_bitmap
	        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

	        block_bitmap_idx = -1;
            //若是直接块
	        if (block_idx < 12) {	 
	              dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
	        } 
            //若是尚未分配一级间接块表
            else if (block_idx == 12) {
                  // 将上面分配的块做为一级间接块表地址
	              dir_inode->i_sectors[12] = block_lba; 
	              block_lba = -1;
	              block_lba = block_bitmap_alloc(cur_part);	       // 再分配一个块做为第0个间接块
	              if (block_lba == -1) {
	                 block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
	                 bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
	                 dir_inode->i_sectors[12] = 0;
	                 printk("alloc block bitmap for sync_dir_entry failed\n");
	                 return false;
	              }

	              block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	              ASSERT(block_bitmap_idx != -1);
                  //每分配一个块就同步一次block_bitmap
	              bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

	              all_blocks[12] = block_lba;
	              //把新分配的第0个间接块地址写入一级间接块表
	              ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
	         } 
               // 若是间接块未分配
            else{
	              all_blocks[block_idx] = block_lba;
	              ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
	         }

	        //再将新目录项p_de写入新分配的间接块
	        memset(io_buf, 0, 512);
	        memcpy(io_buf, p_de, dir_entry_size);
	        ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
	        dir_inode->i_size += dir_entry_size;
	        return true;
      }

      //若第block_idx块已存在,将其读进内存,然后在该块中查找空目录项
      ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      uint8_t dir_entry_idx = 0;
      while (dir_entry_idx < dir_entrys_per_sec) {
            //无论是初始化或是删除文件后,都会将f_type置为FT_UNKNOWN.
	        if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
	           memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);    
	           ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
              dir_inode->i_size += dir_entry_size;
	           return true;
	        }
	        dir_entry_idx++;
      }
      block_idx++;
   }   
   printk("directory is full!\n");
   return false;
}