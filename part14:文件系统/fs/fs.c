#include "fs.h"
#include "stdint.h"
#include "list.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "console.h"
#include "ide.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "file.h"

//默认操作的分区
struct partition* cur_part;

//在分区链表中找到名为arg的分区，将之设置为默认操作分区
static bool mount_partition(struct list_elem* pelem, int arg) {
   char* part_name = (char*)arg;
   struct partition* part = elemtoentry(struct partition, part_tag, pelem);
   //如果找到了
   if(!strcmp(part->name, part_name)){
      cur_part = part;
      struct disk* hd = cur_part->my_disk;

      //sb_buf用来存储从硬盘上读入的超级块
      struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

      //在内存中创建分区cur_part的超级块
      cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
      if(cur_part->sb == NULL) {
         PANIC("alloc memory failed!");
      }

      memset(sb_buf, 0, SECTOR_SIZE);
      ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);   

      memcpy(cur_part->sb, sb_buf, sizeof(struct super_block)); 

      cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
      if (cur_part->block_bitmap.bits == NULL) {
         PANIC("alloc memory failed!");
      }
      cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
      
      ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);   
      

      cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
      if (cur_part->inode_bitmap.bits == NULL) {
         PANIC("alloc memory failed!");
      }
      cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
      
      ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);   

      list_init(&cur_part->open_inodes);
      printk("mount %s done!\n", part->name);
      return true;
   }
   //使list_traversal继续遍历
   return false;
}

/* 格式化分区,也就是初始化分区的元信息,创建文件系统 */
static void partition_format(struct partition* part){
   //引导块需要的扇区数
   uint32_t boot_sector_sects = 1;
   //超级块需要的扇区数
   uint32_t super_block_sects = 1;
   //inode位图需要占据的扇区数量
   uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
   //inode数组需要占据的扇区数量
   uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);
   //上述功能块已经使用的扇区数量
   uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
   //剩余的空闲扇区数量
   uint32_t free_sects = part->sec_cnt - used_sects;  
   //剩余的空闲扇区的位图需要占据的扇区数量
   uint32_t block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
   //把上面用到的那部分也从剩余空闲扇区数里减掉
   uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
   //经过上一步后，确定最终的代表剩余空闲扇区的位图所占据的扇区数量
   block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR); 

   
   /* 超级块初始化 */
   struct super_block sb;
   //做一个小小的揣测，原位魔数应该是作者父母的生日└(^o^)┘，这里改成我写下这行注释的日子吧
   sb.magic = 0x20181219;
   //接着设置超级块所在分区的扇区数、inode数和起始扇区
   sb.sec_cnt = part->sec_cnt;
   sb.inode_cnt = MAX_FILES_PER_PART;
   sb.part_lba_base = part->start_lba;
   //根据600页的图可知，块位图起始扇区地址为本分区的第三块
   sb.block_bitmap_lba = sb.part_lba_base + 2;
   sb.block_bitmap_sects = block_bitmap_sects;
   //inode块位图在空闲块位图之后
   sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
   sb.inode_bitmap_sects = inode_bitmap_sects;
   //inode数组在inode位图之后
   sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
   sb.inode_table_sects = inode_table_sects; 
   //根目录和空闲块区域是用于存储的区域，在分区剩下部分
   sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
   //根目录所在节点号设为0
   sb.root_inode_no = 0;
   sb.dir_entry_size = sizeof(struct dir_entry);

   printk("%s info:\n", part->name);
   printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);

   struct disk* hd = part->my_disk;
   //把超级块写入硬盘
   ide_write(hd, part->start_lba + 1, &sb, 1);
   printk("   super_block_lba:0x%x\n", part->start_lba + 1);
   //我们从空闲块位图、inode位图和inode数组三者中，找占用空间最大的，在堆中申请数据
   uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
   buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
   uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

   //第0个块预留给根目录,位图中先占位
   buf[0] |= 0x01;
   //空闲块区位图所占字节长度
   uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
   //除去以上长度，还剩下多少bit
   uint8_t  block_bitmap_last_bit  = block_bitmap_bit_len % 8;
   //last_size是位图的最后一个扇区中，不足一扇区的其余部分
   uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);

   //将位图占用的扇区中，未使用的bit置1
   memset(&buf[block_bitmap_last_byte], 0xff, last_size);
   uint8_t bit_idx = 0;
   while (bit_idx <= block_bitmap_last_bit) {
      buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
   }
   ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

   memset(buf, 0, buf_size);
   buf[0] |= 0x1;
   ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

   memset(buf, 0, buf_size);  // 先清空缓冲区buf
   struct inode* i = (struct inode*)buf; 
   // .和..
   i->i_size = sb.dir_entry_size * 2;
   // 根目录占inode数组中第0个inode
   i->i_no = 0;
   // 由于上面的memset,i_sectors数组的其它元素都初始化为0 
   i->i_sectors[0] = sb.data_start_lba;
   ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

   memset(buf, 0, buf_size);
   struct dir_entry* p_de = (struct dir_entry*)buf;

   memcpy(p_de->filename, ".", 1);
   p_de->i_no = 0;
   p_de->f_type = FT_DIRECTORY;
   p_de++;

   memcpy(p_de->filename, "..", 2);
   p_de->i_no = 0;
   p_de->f_type = FT_DIRECTORY;

   /* sb.data_start_lba已经分配给了根目录,里面是根目录的目录项 */
   ide_write(hd, sb.data_start_lba, buf, 1);

   printk("   root_dir_lba:0x%x\n", sb.data_start_lba);
   printk("%s format done\n", part->name);
   sys_free(buf);
}

/*解析最上层路径*/
static char* path_parse(char* pathname, char* name_store) {
   if (pathname[0] == '/') {
      //跳过多个连续的字符'/'
      while(*(++pathname) == '/');
   }

   //解析最上层路径
   while (*pathname != '/' && *pathname != 0) {
      *name_store++ = *pathname++;
   }

   if (pathname[0] == 0) { 
      return NULL;
   }
   return pathname; 
}

/* 返回路径深度,比如/a/b/c,深度为3 */
int32_t path_depth_cnt(char* pathname) {
   ASSERT(pathname != NULL);
   char* p = pathname;
   char name[MAX_FILE_NAME_LEN];
   uint32_t depth = 0;

   /* 解析路径,从中拆分出各级名称 */ 
   p = path_parse(p, name);
   while (name[0]) {
      depth++;
      memset(name, 0, MAX_FILE_NAME_LEN);
      if (p){
         p = path_parse(p, name);
      }
   }
   return depth;
}


/* 搜索文件pathname,若找到则返回其inode号,否则返回-1 */
static int search_file(const char* pathname, struct path_search_record* searched_record) {
   //如果待查找的是根目录,直接返回已知根目录信息
   if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
      searched_record->parent_dir = &root_dir;
      searched_record->file_type = FT_DIRECTORY;
      searched_record->searched_path[0] = 0;
      return 0;
   }

   uint32_t path_len = strlen(pathname);
   /* 保证pathname至少是这样的路径/x且小于最大长度 */
   ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
   char* sub_path = (char*)pathname;
   struct dir* parent_dir = &root_dir; 
   struct dir_entry dir_e;

   //记录路径解析出来的各级名称
   char name[MAX_FILE_NAME_LEN] = {0};

   searched_record->parent_dir = parent_dir;
   searched_record->file_type = FT_UNKNOWN;
   //这个变量用于假如查找的pathname是目录，则该变量记录pathname的父级目录
   uint32_t parent_inode_no = 0;
   
   sub_path = path_parse(sub_path, name);
   while (name[0]){
      ASSERT(strlen(searched_record->searched_path) < 512);

      //记录已存在的父目录
      strcat(searched_record->searched_path, "/");
      strcat(searched_record->searched_path, name);

      //在当前目录中找到了name的话 
      if (search_dir_entry(cur_part, parent_dir, name, &dir_e)){
         memset(name, 0, MAX_FILE_NAME_LEN);
         if(sub_path){
            sub_path = path_parse(sub_path, name);
         }
         //如果被打开的是目录
         if (FT_DIRECTORY == dir_e.f_type) {
            parent_inode_no = parent_dir->inode->i_no;
            dir_close(parent_dir);
            parent_dir = dir_open(cur_part, dir_e.i_no);
            searched_record->parent_dir = parent_dir;
            continue;
         } 
         //若是普通文件
         else if (FT_REGULAR == dir_e.f_type) {
            searched_record->file_type = FT_REGULAR;
            return dir_e.i_no;
         }
      } 
      //若找不到,则返回-1
      else{
         return -1;
      }
   }

   dir_close(searched_record->parent_dir);         

   searched_record->parent_dir = dir_open(cur_part, parent_inode_no);      
   searched_record->file_type = FT_DIRECTORY;
   return dir_e.i_no;
}


/* 打开或创建文件成功后,返回文件描述符,否则返回-1 */
int32_t sys_open(const char* pathname, uint8_t flags) {
  //这里只对文件进行打开操作
   if (pathname[strlen(pathname) - 1] == '/') {
      printk("can`t open a directory %s\n",pathname);
      return -1;
   }
   ASSERT(flags <= 7);
   int32_t fd = -1;

   struct path_search_record searched_record;
   memset(&searched_record, 0, sizeof(struct path_search_record));

   //记录目录深度
   uint32_t pathname_depth = path_depth_cnt((char*)pathname);

   //先检查文件是否存在
   int inode_no = search_file(pathname, &searched_record);
   bool found = inode_no != -1 ? true : false; 

   //目录无法使用本函数打开
   if (searched_record.file_type == FT_DIRECTORY) {
      printk("can`t open a direcotry with open(), use opendir() to instead\n");
      dir_close(searched_record.parent_dir);
      return -1;
   }

   uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

   //先判断是否把pathname的各层目录都访问到了,若两者不等，说明中间有某层目录不存在
   if (pathname_depth != path_searched_depth) { 
      printk("cannot access %s: Not a directory, subpath %s is`t exist\n", \
            pathname, searched_record.searched_path);
      dir_close(searched_record.parent_dir);
      return -1;
   }

   //若是在最后一层没找到,并且未指明要创建文件
   if(!found && !(flags & O_CREAT)){
      printk("in path %s, file %s is`t exist\n", \
            searched_record.searched_path, \
            (strrchr(searched_record.searched_path, '/') + 1));
      dir_close(searched_record.parent_dir);
      return -1;
   } 
   //若要创建的文件已存在
   else if(found && (flags & O_CREAT)){
      printk("%s has already exist!\n", pathname);
      dir_close(searched_record.parent_dir);
      flags &= ~O_CREAT;
   }

   switch (flags & O_CREAT) {
      case O_CREAT:
         printk("creating file\n");
         fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
         dir_close(searched_record.parent_dir);
         break;
      default:
         fd = file_open(inode_no, flags);
   }

   /* 此fd是指任务pcb->fd_table数组中的元素下标,
    * 并不是指全局file_table中的下标 */
   return fd;
}

//将文件描述符转为其所在file_table的下标
static uint32_t fd_local_to_global(uint32_t local_fd){
   struct task_struct* cur = get_thread_ptr();
   int32_t global_fd = cur->fd_table[local_fd];  
   ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
   return (uint32_t)global_fd;
} 

//关闭fd文件，成功返回0，失败-1
int32_t sys_close(int32_t fd){
   int32_t res = -1;
   if(fd > 2){
      uint32_t global_fd = fd_local_to_global(fd);
      res = file_close(&file_table[global_fd]);
      get_thread_ptr()->fd_table[fd] = -1;
   }
   return res;
}

/*初始化文件系统*/
void filesys_init() {
   uint8_t channel_no = 0, dev_no, part_idx = 0;

   //sb_buf用来存储从硬盘上读入的超级块
   struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

   if (sb_buf == NULL) {
      PANIC("alloc memory failed!");
   }
   printk("searching filesystem......\n");
   while (channel_no < channel_cnt){
         dev_no = 0;
         while(dev_no < 2){
            if(dev_no == 0){
               printk("skip hd60M.img\n");
               dev_no++;
               continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->prim_parts;
            while(part_idx < 12) {
               if (part_idx == 4) {
                  part = hd->logic_parts;
               }
               if (part->sec_cnt != 0) {
                  memset(sb_buf, 0, SECTOR_SIZE);

                  //读出分区的超级块,根据魔数是否正确来判断是否存在文件系统
                  ide_read(hd, part->start_lba + 1, sb_buf, 1);   

                  //只支持自己的文件系统.若磁盘上已经有文件系统就不再格式化了
                  if(sb_buf->magic == 0x20181219) {
                     printk("%s has filesystem\n", part->name);
                  }
                  //其它文件系统不支持,一律按无文件系统处理
                  else {  
                     printk("formatting %s`s partition %s......\n", hd->name, part->name);
                     partition_format(part);
                  }
               }
               part_idx++;
               //下一分区
               part++;
            }
            //下一磁盘
            dev_no++;
         }
         //下一通道
         channel_no++;
   }
   sys_free(sb_buf);

   //挂载分区
   char default_part[8] = "sdb1";
   list_traversal(&partition_list, mount_partition, (int)default_part);

   //打开当前分区的根目录
   open_root_dir(cur_part);

   //初始化文件表
   uint32_t fd_idx = 0;
   while (fd_idx < MAX_FILE_OPEN) {
      file_table[fd_idx++].fd_inode = NULL;
   }
}

//write系统调用，将buf中连续count个字节写入到文件描述符所代表文件中
//成功返回写入的字节数，失败返回-1
int32_t sys_write(int32_t fd, const void* buf, uint32_t count){
   if(fd < 0){
      printk("sys_write: fd error\n");
      return -1;
   }
   //如果是向屏幕输出
   if(fd == stdout_no){  
      char tmp_buf[1024] = {0};
      memcpy(tmp_buf, buf, count);
      console_put_str(tmp_buf);
      return count;
   }
   //转换为全局描述符表中该文件的下表
   uint32_t _fd = fd_local_to_global(fd);
   struct file* wr_file = &file_table[_fd];
   //只有设置为只写或读写才能写文件，只单纯创建不可以
   if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
      uint32_t bytes_written  = file_write(wr_file, buf, count);
      return bytes_written;
   } 
   else{
      console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
      return -1;
   }
}

//从文件fd中，读取count个字节到buf中，成功返回读出的字节数
int32_t sys_read(int32_t fd, void* buf, uint32_t count){
   if(fd < 0){
      printk("sys_read: fd error\n");
      return -1;
   }
   ASSERT(buf != NULL);
   uint32_t _fd = fd_local_to_global(fd);
   return file_read(&file_table[_fd], buf, count);
}

