#include <stdio.h>
#include <unistd.h>
#include <math_extra.h>

int main(void)
{
    printf("=== Dynamic Library Test ===\n");

    printf("printf is at: 0x%x\n", (unsigned int)(unsigned long)printf);
    printf("This app is dynamically linked!\n");

    int x = 5;
    printf("square(%d) = %d\n", x, square(x));
    printf("cube(%d) = %d\n", x, cube(x));
    printf("factorial(%d) = %d\n", x, factorial(x));

    printf("PID: %d\n", getpid());

    printf("=== Dynamic Library Test Passed! ===\n");
    return 0;
}
