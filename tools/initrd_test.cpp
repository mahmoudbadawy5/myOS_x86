#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>

//typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
//typedef unsigned long long uint64_t;

//typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
//typedef long long int64_t;

using namespace std;

struct initrd_header{
    uint32_t magic;
} __attribute__((packed));

struct file_node{
    uint8_t flags;
    uint32_t start;
    uint32_t size;
    char name[128];
} __attribute__((packed));

struct dir_node{
    uint32_t entities;
    file_node** nodes;
} __attribute__((packed));

int main()
{
    FILE* initrd = fopen("initrd.img", "rb");
    initrd_header *head=(initrd_header*)malloc(sizeof(initrd_header));
    fread(head, sizeof(*head), 1, initrd);
    cout << "Header: " << (head->magic) << " : ";
    for(int i=0;i<4;i++)
    {
        char cur=((head->magic) & (0xFF << (i*8))) >> (i*8);
        cout << cur;
    }
    cout << endl;
    dir_node* root=(dir_node*)malloc(sizeof(dir_node));
    fread(&root->entities, sizeof(uint32_t), 1, initrd);
    printf("Root has %d entities\n", (int)root->entities);
    root->nodes = new file_node*[root->entities];
    for(int i=0;i<root->entities;i++)
    {
        root->nodes[i] = new file_node;
        fread(root->nodes[i], sizeof(file_node), 1, initrd);
        printf("%d %d %d %s\n", root->nodes[i]->flags, root->nodes[i]->start, root->nodes[i]->size, root->nodes[i]->name);
    }
    fseek(initrd, 701, SEEK_SET);
    char f[33];
    f[29]='\0';
    fread(f, 29, 1, initrd);
    cout << f << endl;
}