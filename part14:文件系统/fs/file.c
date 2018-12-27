#include "file.h"
#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "string.h"
#include "thread.h"
#include "global.h"

#define DEFAULT_SECS 1

/*文件表*/
struct file file_table[MAX_FILE_OPEN];

/*从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1*/
int32_t get_free_slot_in_global(void) {
   uint32_t fd_idx = 3;
   while (fd_idx < MAX_FILE_OPEN) {
      if (file_table[fd_idx].fd_inode == NULL) {
	     break;
      }
      fd_idx++;
   }
   if(fd_idx == MAX_FILE_OPEN){
      printk("exceed max open files\n");
      return -1;
   }
   return fd_idx;
}

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,
 * 成功返回下标,失败返回-1 */
int32_t pcb_fd_install(int32_t globa_fd_idx){
   struct task_struct* cur = get_thread_ptr();
   uint8_t local_fd_idx = 3; // 跨过stdin,stdout,stderr
   while(local_fd_idx < MAX_FILES_OPEN_PER_PROC) {
      if(cur->fd_table[local_fd_idx] == -1) {	// -1表示free_slot,可用
	     cur->fd_table[local_fd_idx] = globa_fd_idx;
	     break;
      }
      local_fd_idx++;
   }
   if (local_fd_idx == MAX_FILES_OPEN_PER_PROC) {
      printk("exceed max open files_per_proc\n");
      return -1;
   }
   return local_fd_idx;
}

/* 分配一个i结点,返回i结点号 */
int32_t inode_bitmap_alloc(struct partition* part) {
   int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
   if (bit_idx == -1) {
      return -1;
   }
   bitmap_set(&part->inode_bitmap, bit_idx, 1);
   return bit_idx;
}
   
/* 分配1个扇区,返回其扇区地址 */
int32_t block_bitmap_alloc(struct partition* part) {
   int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
   if (bit_idx == -1) {
      return -1;
   }
   bitmap_set(&part->block_bitmap, bit_idx, 1);
   //返回具体可用的扇区地址
   return (part->sb->data_start_lba + bit_idx);
} 

/*更新硬盘中的bitmap，将内存中bitmap第bit_idx位所在的512字节同步到硬盘*/
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp_type) {
   //本i结点索引相对于位图的扇区偏移量
   uint32_t off_sec = bit_idx / 4096;
   //本i结点索引相对于位图的字节偏移量
   uint32_t off_size = off_sec * BLOCK_SIZE;
   uint32_t sec_lba;
   uint8_t* bitmap_off;

   switch (btmp_type) {
      case INODE_BITMAP:
	        sec_lba = part->sb->inode_bitmap_lba + off_sec;
	        bitmap_off = part->inode_bitmap.bits + off_size;
	        break;
      case BLOCK_BITMAP: 
	        sec_lba = part->sb->block_bitmap_lba + off_sec;
	        bitmap_off = part->block_bitmap.bits + off_size;
	        break;
   }
   ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

/* 创建文件,若成功则返回文件描述符,否则返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
   //公共缓冲区
   void* io_buf = sys_malloc(1024);
   if(io_buf == NULL){
      printk("in file_creat: sys_malloc for io_buf failed\n");
      return -1;
   }

   //操作失败时回滚各资源的状态位
   uint8_t rollback_step = 0;

   //分配inode
   int32_t inode_no = inode_bitmap_alloc(cur_part);
   if(inode_no == -1){
      printk("in file_creat: allocate inode failed\n");
      return -1;
   }

   struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
   if (new_file_inode == NULL) {
      printk("file_create: sys_malloc for inode failded\n");
      rollback_step = 1;
      goto rollback;
   }
   //初始化i结点
   inode_init(inode_no, new_file_inode);

   //返回的是file_table数组的下标
   int fd_idx = get_free_slot_in_global();
   if (fd_idx == -1){
      printk("exceed max open files\n");
      rollback_step = 2;
      goto rollback;
   }

   file_table[fd_idx].fd_inode = new_file_inode;
   file_table[fd_idx].fd_pos = 0;
   file_table[fd_idx].fd_flag = flag;
   file_table[fd_idx].fd_inode->write_deny = false;

   struct dir_entry new_dir_entry;
   memset(&new_dir_entry, 0, sizeof(struct dir_entry));

   create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

   if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
      printk("sync dir_entry to disk failed\n");
      rollback_step = 3;
      goto rollback;
   }

   memset(io_buf, 0, 1024);
   //将父目录i结点的内容写回到硬盘
   inode_sync(cur_part, parent_dir->inode, io_buf);

   memset(io_buf, 0, 1024);
   //将新创建文件的i结点内容写回到硬盘
   inode_sync(cur_part, new_file_inode, io_buf);

   //将inode_bitmap位图写回到硬盘
   bitmap_sync(cur_part, inode_no, INODE_BITMAP);

   //将创建的文件i结点添加到open_inodes链表
   list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
   new_file_inode->i_open_cnts = 1;    

   sys_free(io_buf);
   return pcb_fd_install(fd_idx);

/*创建文件需要创建相关的多个资源,若某步失败则会执行到下面的回滚步骤 */
rollback:
   switch (rollback_step) {
      case 3:
	 /* 失败时,将file_table中的相应位清空 */
	 memset(&file_table[fd_idx], 0, sizeof(struct file)); 
      case 2:
	 sys_free(new_file_inode);
      case 1:
	 /* 如果新文件的i结点创建失败,之前位图中分配的inode_no也要恢复 */
	 bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
	 break;
   }
   sys_free(io_buf);
   return -1;
}


int32_t file_open(uint32_t inode_no, uint8_t flag){
   int fd_idx = get_free_slot_in_global();
   if(fd_idx == -1){
      printk("exceed max open files\n");
      return -1;
   }
   //给文件表中该文件结构体初始化
   file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
   file_table[fd_idx].fd_pos = 0;
   file_table[fd_idx].fd_flag = flag;
   //记录该文件是否被别的进程打开
   bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

   //如果是只写或读写，因为涉及到写，所以需要判断文件是否正在被使用
   if (flag & O_WRONLY || flag & O_RDWR){
      enum intr_status old_status = intr_disable();
      //未被使用
      if (!(*write_deny)) {
         *write_deny = true;
         intr_set_status(old_status);
      }
      //否则，该文件被其他文件所持
      else{
         intr_set_status(old_status);
         printk("file can`t be write now, try again later\n");
         return -1;
      }
   }
   //将全局描述符转为进程自己的fd号
   return pcb_fd_install(fd_idx);
}

int32_t file_close(struct file* f){
   if(f == NULL)
      return -1;
   f->fd_inode->write_deny = false;
   inode_close(f->fd_inode);
   //之前file_table的是否为空标准为fd_inode是否为NULL
   f->fd_inode = NULL;
   return 0;
}

int32_t file_write(struct file* file, const void* buf, uint32_t count){
   //若当前文件大小在写入buf中count大小数据后，超过文件支持大小则返回
   //文件目前最大只支持512*140=71680字节
   if ((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140))  { 
      printk("exceed max file_size 71680 bytes, write file failed\n");
      return -1;
   }
   //申请一块缓冲区
   uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
   if (io_buf == NULL) {
      printk("file_write: sys_malloc for io_buf failed\n");
      return -1;
   }
   //all_block中装有所有的块地址，12个直接块指针，128个一级块指针
   uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);   // 用来记录文件所有的块地址
   if (all_blocks == NULL) {
      printk("file_write: sys_malloc for all_blocks failed\n");
      return -1;
   }
   //用src作为操作指针
   const uint8_t* src = buf; 
   //已写入数据大小
   uint32_t bytes_written = 0;
   //未写入数据大小  
   uint32_t size_left = count;
   //块地址
   int32_t block_lba = -1;
   //用来记录block对应于block_bitmap中的索引,做为参数传给bitmap_sync
   uint32_t block_bitmap_idx = 0;
   //用来索引扇区
   uint32_t sec_idx;
   //扇区地址
   uint32_t sec_lba;
   //扇区内字节偏移量
   uint32_t sec_off_bytes;
   //扇区内剩余字节量
   uint32_t sec_left_bytes;
   //每次写入硬盘的数据块大小
   uint32_t chunk_size;
   //用来获取一级间接表地址
   int32_t indirect_block_table;
   //块索引
   uint32_t block_idx;

   //判断文件是否是第一次写
   if(file->fd_inode->i_sectors[0] == 0){
      //分配一个块
      block_lba = block_bitmap_alloc(cur_part);
      if(block_lba == -1){
         printk("file_write: block_bitmap_alloc failed\n");
         return -1;
      }
      file->fd_inode->i_sectors[0] = block_lba;

      //每分配一个块就将位图实时写回到硬盘
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      ASSERT(block_bitmap_idx != 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
   }
   //分别计算写入前和写入后，文件的块数，并判断是否需要申请新扇区
   uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
   uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
   ASSERT(file_will_use_blocks <= 140);
   uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
   //不需要申请新扇区，
   if(add_blocks == 0){
      //文件数据量将在12块之内
      if(file_has_used_blocks <= 12){
         //指向最后一个已有数据的扇区
         block_idx = file_has_used_blocks - 1;
         all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      } 
      //使用了间接块
      else{
         ASSERT(file->fd_inode->i_sectors[12] != 0);
         //先读出一级块地址
         indirect_block_table = file->fd_inode->i_sectors[12];
         //把一级块读到all_block里
         ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   }
   //写入count个字节需要申请新的扇区的话
   else{
      if(file_will_use_blocks <= 12){
         //count个字节需要分成两部分写入到两个或多个扇区扇区
         //现将第一个扇区写入到all_blocks
         block_idx = file_has_used_blocks - 1;
         ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
         all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
         //指向第一个要分配的新扇区
         block_idx = file_has_used_blocks;
         while(block_idx < file_will_use_blocks){
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba == -1){
               printk("file_write: block_bitmap_alloc for situation 1 failed\n");
               return -1;
            }
            ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
            file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

            /* 每分配一个块就将位图同步到硬盘 */
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            block_idx++;   // 下一个分配的新扇区
         }
      }
      //第二种情况: 旧数据在12个直接块内,新数据将使用间接块
      else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12){
         block_idx = file_has_used_blocks - 1;
         all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

         //创建一级间接块表
         block_lba = block_bitmap_alloc(cur_part);
         if(block_lba == -1){
            printk("file_write: block_bitmap_alloc for situation 2 failed\n");
            return -1;
         }
         ASSERT(file->fd_inode->i_sectors[12] == 0);
         //分配一级间接块索引表
         indirect_block_table = block_lba;
         file->fd_inode->i_sectors[12] = block_lba;

         block_idx = file_has_used_blocks;
         while (block_idx < file_will_use_blocks) {
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
               printk("file_write: block_bitmap_alloc for situation 2 failed\n");
               return -1;
            }
            //如果是直接块，则将获取的块地址直接放到all_blocks里
            if(block_idx < 12){
               //确保尚未分配扇区地址
               ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
               file->fd_inode->i_sectors[block_idx] = block_lba;
               all_blocks[block_idx] = block_lba;
            }
            //间接块只写入到all_block数组中,待全部分配完成后一次性同步到硬盘
            else{
               all_blocks[block_idx] = block_lba;
            }
            //依旧每分配一个块就将位图同步到硬盘
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            
            block_idx++;
         }
         //统一把一级块上的块表同步到硬盘
         ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
      //第三种情况是旧数据新数据都在一级块中
      else if (file_has_used_blocks > 12) {
         //已经具备了一级间接块表
         ASSERT(file->fd_inode->i_sectors[12] != 0);
         //获取一级间接表地址
         indirect_block_table = file->fd_inode->i_sectors[12];

         //把一级块读进来
         ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1); 

         block_idx = file_has_used_blocks;     
         while(block_idx < file_will_use_blocks){
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba == -1){
               printk("file_write: block_bitmap_alloc for situation 3 failed\n");
               return -1;
            }
            all_blocks[block_idx++] = block_lba;
            //依旧每分配一个块就将位图同步到硬盘
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
         }
         ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);   // 同步一级间接块表到硬盘
      } 
   }
   //判断是否含有剩余扇区的标志
   bool first_write_block = true;
   //准备完毕，开始写数据
   //置fd_pos为文件的最后，方便写入
   file->fd_pos = file->fd_inode->i_size - 1;
   while(bytes_written < count){
      memset(io_buf, 0, BLOCK_SIZE);
      sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
      sec_lba = all_blocks[sec_idx];
      sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
      sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

      chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
      //若是第一次读取，都需要将该块读出来，拷贝到io_buf中
      if(first_write_block){
         ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
         first_write_block = false;
      }
      memcpy(io_buf + sec_off_bytes, src, chunk_size);
      ide_write(cur_part->my_disk, sec_lba, io_buf, 1);

      src += chunk_size;
      file->fd_inode->i_size += chunk_size;
      file->fd_pos += chunk_size;   
      bytes_written += chunk_size;
      size_left -= chunk_size;
   }
   //同步inode
   inode_sync(cur_part, file->fd_inode, io_buf);
   sys_free(all_blocks);
   sys_free(io_buf);
   return bytes_written;
}

int32_t file_read(struct file* file, void* buf, uint32_t count){
   uint8_t* buf_dst = (uint8_t*)buf;
   uint32_t size = count, size_left = size;
   //若文件剩余的可读字节数少于count, 就用剩余量做为count */
   if ((file->fd_pos + count) > file->fd_inode->i_size)  {
      size = file->fd_inode->i_size - file->fd_pos;
      size_left = size;
      if (size == 0) {     // 若到文件尾则返回-1
         return -1;
      }
   }
   uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
   if(io_buf == NULL){
      printk("file_read: sys_malloc for io_buf failed\n");
      return -1;
   }
   //用来记录文件所有的块地址，一共需要存放128+12个块地址
   uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
   if (all_blocks == NULL) {
      printk("file_read: sys_malloc for all_blocks failed\n");
      return -1;
   }

   //数据所在块的起始地址
   uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;    
   //数据所在块的终止地址
   uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
   //如增量为0,表示数据在同一扇区
   uint32_t read_blocks = block_read_start_idx - block_read_end_idx;
   ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);
   //用来获取一级间接表地址
   int32_t indirect_block_table;
   //获取待读的块地址
   uint32_t block_idx;

   //增量为0，,不涉及到跨扇区读取
   if (read_blocks == 0){
      ASSERT(block_read_end_idx == block_read_start_idx);
      //待读的数据在12个直接块之内
      if(block_read_end_idx < 12){
         block_idx = block_read_end_idx;
         all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      }
      //若用到了一级间接块表,需要将表中间接块读进来
      else{
         indirect_block_table = file->fd_inode->i_sectors[12];
         ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   } 
   //要读多个块
   else{
      //若起始块终止块都是直接块
      if(block_read_end_idx < 12 ){
         //数据结束所在的块属于直接块
         block_idx = block_read_start_idx; 
         while(block_idx <= block_read_end_idx){
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx]; 
            block_idx++;
         }
      }
      //起始块是直接块，终止块在一级间接块中
      else if(block_read_start_idx < 12 && block_read_end_idx >= 12){
         block_idx = block_read_start_idx;
         while (block_idx < 12) {
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
            block_idx++;
         }
         //确保已经分配了一级间接块表
         ASSERT(file->fd_inode->i_sectors[12] != 0);

         //再将一级间接块地址写入all_blocks
         indirect_block_table = file->fd_inode->i_sectors[12];
         ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);       // 将一级间接块表读进来写入到第13个块的位置之后
      } 
      //起始块终止块都在一级间接块中
      else{
         ASSERT(file->fd_inode->i_sectors[12] != 0);
         //获取一级间接表地址
         indirect_block_table = file->fd_inode->i_sectors[12];
         ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);       // 将一级间接块表读进来写入到第13个块的位置之后
      } 
   }

   //用到的块地址已经收集到all_blocks中,下面开始读数据
   uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
   uint32_t bytes_read = 0;
   while (bytes_read < size) {
      sec_idx = file->fd_pos / BLOCK_SIZE;
      sec_lba = all_blocks[sec_idx];
      sec_off_bytes = file->fd_pos % BLOCK_SIZE;
      sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
      
      chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;        // 待读入的数据大小

      memset(io_buf, 0, BLOCK_SIZE);
      ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
      memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);

      buf_dst += chunk_size;
      file->fd_pos += chunk_size;
      bytes_read += chunk_size;
      size_left -= chunk_size;
   }
   sys_free(all_blocks);
   sys_free(io_buf);
   return bytes_read;
}

