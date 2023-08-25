#include <fs/vfs.h>
#include <mem/malloc.h>
#include <fs/initrd.h>
#include <stdio.h>
#include <string.h>

initrd_header_t *fs_initrd_header;
initrd_directory_t *fs_initrd_root;
uint32_t initrd_location;

// impl is location in memory

uint32_t initrd_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    if ((node->flags & FS_DIRECTORY) == FS_DIRECTORY)
        return 0;
    for (int i = 0; i < size * units; i++)
    {
        buffer[i] = *(char *)(node->impl + node->seek_offset + i);
    }
    node->seek_offset += size * units;
    return units;
}

void initrd_seek_fs(fs_node_t *node, uint32_t offset, uint8_t type)
{
    if (type == SEEK_START)
        node->seek_offset = offset;
    else if (type == SEEK_CUR)
        node->seek_offset += offset;
    else
        node->seek_offset = node->size - offset;
}

dirent_t *initrd_readdir_fs(fs_node_t *node)
{
    if ((node->flags & FS_DIRECTORY) != FS_DIRECTORY)
        return NULL;
    initrd_directory_t *dir = initrd_loaddir(node->impl);
    dirent_t *ret = malloc(sizeof(dirent_t));
    ret->file_count = dir->num_entites;
    ret->files = malloc(sizeof(char *) * ret->file_count);
    for (int i = 0; i < dir->num_entites; i++)
    {
        initrd_directory_entity_t *entity = dir->entites[i];
        ret->files[i] = entity->name;
    }
    return ret;
}

fs_node_t *initrd_finddir_fs(fs_node_t *node, char *name)
{
    if ((node->flags & FS_DIRECTORY) != FS_DIRECTORY)
        return NULL;
    initrd_directory_t *dir = initrd_loaddir(node->impl);
    for (int i = 0; i < dir->num_entites; i++)
    {
        if (strcmp(dir->entites[i]->name, name) == 0)
        {
            if (dir->entites[i]->flags & FS_FILE)
                return initrd_loadfile_fs_node(dir->entites[i]->name, dir->entites[i]->start, dir->entites[i]->size);
            else
                return initrd_loaddir_fs_node(dir->entites[i]->name, dir->entites[i]->start);
        }
    }
    return NULL;
}

fs_node_t *initrd_loadfile_fs_node(char *name, uint32_t start, uint32_t size)
{
    fs_node_t *file = malloc(sizeof(fs_node_t));
    file->flags = FS_FILE;
    strcpy(file->name, name);
    file->impl = start + initrd_location;
    file->size = size;
    file->read = initrd_read_fs;
    file->seek = initrd_seek_fs;
    file->seek_offset = 0;
    return file;
}

fs_node_t *initrd_loaddir_fs_node(char *name, uint32_t start)
{
    fs_node_t *dir = malloc(sizeof(fs_node_t));
    dir->flags = FS_DIRECTORY;
    strcpy(dir->name, name);
    dir->impl = start + initrd_location;
    dir->readdir = initrd_readdir_fs;
    dir->finddir = initrd_finddir_fs;
    return dir;
}

initrd_directory_t *initrd_loaddir(uint32_t location)
{
    initrd_directory_t *dir = malloc(sizeof(initrd_directory_entity_t));
    dir->num_entites = *(uint32_t *)location;
    location += sizeof(dir->num_entites);
    dir->entites = malloc(sizeof(initrd_directory_entity_t *) * dir->num_entites);
    for (int i = 0; i < dir->num_entites; i++)
    {
        dir->entites[i] = (initrd_directory_entity_t *)(location + sizeof(initrd_directory_entity_t) * i);
    }
    return dir;
}

void init_initrd(uint32_t initrd_start)
{
    initrd_location = initrd_start;
    fs_initrd_header = (initrd_header_t *)initrd_start;
    initrd_start += sizeof(initrd_header_t);
    fs_initrd_root = initrd_loaddir(initrd_start);
    fs_node_t *root_fs = initrd_loaddir_fs_node("/", initrd_start - initrd_location);
    init_vfs(root_fs);
}