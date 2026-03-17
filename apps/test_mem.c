#include <test.h>

int arr[10005];

void main()
{
    arr[0]=17;
    arr[10004] = 15;
    test_syscall0(arr[0]);
    test_syscall0(arr[1]);
    test_syscall0(arr[10004]);
    test_syscall0(arr[10005]);
    test_syscall0(arr[10006]);
}