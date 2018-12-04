#include "init.h"
#include "print.h"
#include "debug.h"
#include "memory.h"

int main(){
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    
	init_all();

	void* addr = get_kernel_pages(3);
	put_str("\n get_kernel_page start vaddr:");
	put_int((uint32_t)addr);
	put_str("\n");
    
    while(1);
    return 0;
}
