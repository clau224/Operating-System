#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "stdint.h"

/* 超级块 */
struct super_block {
   //魔数，文件系统类型判定的标志
   uint32_t magic;
   //超级块表示的分区的扇区数
   uint32_t sec_cnt;
   //inode数量
   uint32_t inode_cnt;
   //本分区的起始lba地址
   uint32_t part_lba_base;

   //块位图起始扇区地址
   uint32_t block_bitmap_lba;
   //块位图占用的扇区数
   uint32_t block_bitmap_sects;

   //i结点位图起始扇区地址
   uint32_t inode_bitmap_lba;
   //i结点位图占用的扇区数量
   uint32_t inode_bitmap_sects;

   //inode数组起始扇区lba地址
   uint32_t inode_table_lba;
   //inode数组占用的扇区数量
   uint32_t inode_table_sects;

   //数据区起始扇区地址
   uint32_t data_start_lba;
   //根目录所在的I结点号
   uint32_t root_inode_no;
   //目录项大小
   uint32_t dir_entry_size;

   //凑够一个块，这里一个块作者定义的是一个扇区
   uint8_t  pad[460];
   //避免结构体成员对齐
} __attribute__ ((packed));

#endif