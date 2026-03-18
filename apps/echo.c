#include <test.h>

void main()
{
    while(1) {
        char x[2] = {0, 0};
        if(read(x, 2)) {
            print(x);
        }
    }
}