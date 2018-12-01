#include "init.h"
#include "print.h"
#include "debug.h"

int main(void) {
    put_str("Hello,World!\n");
    put_str("I am kernel\n");
    init_all();
    ASSERT(1 == 0);
    while(1);
    return 0;
}
