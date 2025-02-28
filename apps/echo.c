#include <test.h>

void main()
{
    char buf[2] = {};
    while(1)
    {
        if(read(buf, 1)) print(buf);
    }
}