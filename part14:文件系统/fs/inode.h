#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"
#include "ide.h"

/* inode结构 */
struct inode {
	   //inode编号
   	uint32_t i_no;

	   //表示文件大小或该目录下所有目录项大小之和，单位字节
   	uint32_t i_size;

   	//记录该文件被打开次数
   	uint32_t i_open_cnts;
   	//避免同时对该文件写入，每次写入需检查该标志位
   	bool write_deny;

   	//11个直接块，1个一级间接块，作者没有实现二级和三级
   	uint32_t i_sectors[13];
   	//在inode链表中的位置
   	struct list_elem inode_tag;
};

struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_close(struct inode* inode);

#endif