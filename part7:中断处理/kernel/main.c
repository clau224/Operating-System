#include "init.h"
#include "print.h"
void main(void) {
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    init_all();
    asm volatile("sti");
    while(1);
}
