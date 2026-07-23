#include <test.h>


int main()
{
    int words = 0;
    while(1)
    {
        words++;
        if(words >= 20000) {
            words = 0;
            print("Hello\n");
        }
        //print("Hello");
    }
}