#include <test.h>

int arr[10005];

int main()
{
    arr[0]=17;
    arr[10004] = 15;
    test_syscall0(arr[0]);
    test_syscall0(arr[1]);
    test_syscall0(arr[10004]);
    test_syscall0(arr[10005]);
    test_syscall0(arr[10006]);

    unsigned int* new_heap = (unsigned int*) sbrk(15);
    new_heap[0] = 122;
    new_heap[1] = 124;
    new_heap[14] = 111;

    test_syscall0(new_heap[0]);
    test_syscall0(new_heap[1]);
    test_syscall0(new_heap[2]);
    test_syscall0(new_heap[14]);    
}