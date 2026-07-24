#include <stdio.h>
#include <syscalls.h>

int main(void)
{
    printf("=== Dynamic Library Test ===\n");
    printf("This app is dynamically linked!\n");
    printf("PID: %d\n", sys_getpid());
    printf("=== Dynamic Library Test Passed! ===\n");
    return 0;
}
