#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"
#include "memory.h"
#include "fs.h"
#include "sync.h"
#include "file.h"
#include "string.h"

void kernel_thread_a(void*);
void kernel_thread_b(void*);
void user_prog_a(void);
void user_prog_b(void);
int32_t k_sync_printf(char* format, ...);
int32_t u_sync_printf(char* format, ...);

struct lock printlock;

int main(void){
   put_str("I am kernel\n");
   init_all();
   lock_init(&printlock);
   //process_execute(user_prog_a, "user_prog_a");
   //process_execute(user_prog_b, "user_prog_b");
   //thread_start("k_thread_a", 31, kernel_thread_a, "I am thread_a");
   //__asm__("xchg %%bx,%%bx" : : );
   //thread_start("k_thread_b", 31, kernel_thread_b, "I am thread_b");
   //__asm__("xchg %%bx,%%bx" : : );

   uint32_t fd1 = sys_open("/file1", O_CREAT | O_RDWR);
   k_sync_printf("open file1: fd = %d\n", fd1);

   //uint32_t fd2 = sys_open("/file2", O_CREAT | O_RDWR);
   //k_sync_printf("open file2: fd = %d\n", fd2);

   sys_write(fd1, "Hello World\n", 12);
   k_sync_printf("write file1: Hello World\n");
   sys_write(fd1, "I am kernel\n", 12);
   k_sync_printf("write file1: I am kernel\n");

   //sys_close(fd2);
   //k_sync_printf("close: fd =%d\n", fd2);

   sys_close(fd1);
   k_sync_printf("close: fd =%d\n", fd1);


   char* buf = malloc(60*sizeof(char));

   memset(buf, 0, 60);
   uint32_t fd = sys_open("/file1", O_RDWR);
   int a = sys_read(fd, buf, 12);
   k_sync_printf("read file1(%d characters): %s", a, buf);

   memset(buf, 0, 60);
   a = sys_read(fd, buf, 12);
   k_sync_printf("read file1(%d characters): %s", a, buf);

   memset(buf, 0, 60);
   sys_lseek(fd, 0, SEEK_SET);
   a = sys_read(fd, buf, 12);
   k_sync_printf("read file1(%d characters): %s", a, buf);

   while(1);
   return 0;
}

int32_t k_sync_printf(char* format, ...){
   va_list args;
   va_start(args, format);
   char buf[1024] = {0};
   vsprintf(buf, format, args);
   va_end(args);
   lock_acquire(&printlock);
   int32_t ret = sys_write(stdout_no, buf, strlen(buf));
   lock_release(&printlock);
   return ret;
}

int32_t u_sync_printf(char* format, ...){
   va_list args;
   va_start(args, format);
   char buf[1024] = {0};
   vsprintf(buf, format, args);
   va_end(args);
   lock(&printlock);
   int32_t ret = write(stdout_no, buf, strlen(buf));
   unlock(&printlock);
   return ret;
}


void kernel_thread_a(void* arg) {     
   void* addr1 = sys_malloc(1025);
   void* addr2 = sys_malloc(1026);
   void* addr3 = sys_malloc(1027);
   k_sync_printf(" thread_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

void kernel_thread_b(void* arg) {     
   void* addr1 = sys_malloc(1024);
   void* addr2 = sys_malloc(1023);
   void* addr3 = sys_malloc(1022);
   k_sync_printf(" thread_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

void user_prog_a(void) {
   void* addr1 = malloc(1600);
   void* addr2 = malloc(3211);
   void* addr3 = malloc(6411);
   u_sync_printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}


void user_prog_b(void) {
   void* addr1 = malloc(1711);
   void* addr2 = malloc(3311);
   void* addr3 = malloc(6511);
   u_sync_printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}
