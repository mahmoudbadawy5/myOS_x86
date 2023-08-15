#pragma once

#include <types.h>

typedef struct initrd_header
{
    char magic[4];
} __attribute__((packed)) initrd_header_t;

typedef struct initrd_directory_entity
{
    uint8_t flags;
    uint32_t start;
    uint32_t size;
    char name[128];
} __attribute__((packed)) initrd_directory_entity_t;

typedef struct initrd_directory
{
    uint32_t num_entites;
    initrd_directory_entity_t **entites;

} __attribute__((packed)) initrd_directory_t;

void init_initrd(uint32_t initrd_start);
uint32_t initrd_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer);
void initrd_seek_fs(fs_node_t *node, uint32_t offset, uint8_t type);
dirent_t *initrd_readdir_fs(fs_node_t *node);
fs_node_t *initrd_finddir_fs(fs_node_t *node, char *name);
fs_node_t *initrd_loadfile_fs_node(char *name, uint32_t start, uint32_t size);
fs_node_t *initrd_loaddir_fs_node(char *name, uint32_t start);
initrd_directory_t *initrd_loaddir(uint32_t location);
