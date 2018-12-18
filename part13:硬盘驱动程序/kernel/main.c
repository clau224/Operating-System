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

void kernel_thread_a(void*);
void kernel_thread_b(void*);
void user_prog_a(void);
void user_prog_b(void);


int main(void) {
   put_str("I am kernel\n");
   init_all();
   intr_enable();
   process_execute(user_prog_a, "user_prog_a");
   process_execute(user_prog_b, "user_prog_b");
   thread_start("k_thread_a", 31, kernel_thread_a, "I am thread_a");
   thread_start("k_thread_b", 15, kernel_thread_b, "I am thread_b");

   while(1);
   return 0;
}


void kernel_thread_a(void* arg) {     
   void* addr1 = sys_malloc(1025);
   void* addr2 = sys_malloc(1026);
   void* addr3 = sys_malloc(1027);
   console_put_str(" thread_a malloc addr:0x");
   console_put_int((int)addr1);
   console_put_str(",0x");
   console_put_int((int)addr2);
   console_put_str(",0x");
   console_put_int((int)addr3);
   console_put_char('\n');

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
   console_put_str(" thread_b malloc addr:0x");
   console_put_int((int)addr1);
   console_put_str(",0x");
   console_put_int((int)addr2);
   console_put_str(",0x");
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

void user_prog_a(void) {
   void* addr1 = malloc(16);
   void* addr2 = malloc(32);
   void* addr3 = malloc(64);
   printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}


void user_prog_b(void) {
   void* addr1 = malloc(17);
   void* addr2 = malloc(33);
   void* addr3 = malloc(65);
   printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}