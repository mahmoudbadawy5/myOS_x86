#pragma once

#include <types.h>

#define FS_FILE 0x01
#define FS_DIRECTORY 0x02
#define FS_CHARDEVICE 0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE 0x05
#define FS_SYMLINK 0x06
#define FS_MOUNTPOINT 0x08 // Is the file an active mountpoint?

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node *, uint32_t, uint32_t, uint8_t *);  // File, unit_size, num_units, buf
typedef uint32_t (*write_type_t)(struct fs_node *, uint32_t, uint32_t, uint8_t *); // File, unit_size, num_units, buf
typedef void (*seek_type_t)(struct fs_node *, uint32_t, uint8_t);                  // File, offset, seek_type
typedef void (*open_type_t)(struct fs_node *);                                     // File
typedef void (*close_type_t)(struct fs_node *);                                    // File
typedef struct dirent *(*readdir_type_t)(struct fs_node *);                        // Dir,  List dir (Gets names of files)
typedef struct fs_node *(*finddir_type_t)(struct fs_node *, char *name);           //

typedef struct fs_node
{
    char name[128];
    uint8_t flags;
    uint32_t size;
    uint32_t inode;
    uint32_t seek_offset;
    uint32_t impl;
    struct fs_node *ptr;
    read_type_t read;
    write_type_t write;
    seek_type_t seek;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
} fs_node_t;

typedef struct dirent
{
    char **files;
    uint32_t file_count;
} dirent_t;

void init_vfs(fs_node_t *root);
uint32_t read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer);
void seek_fs(fs_node_t *node, uint32_t offset, uint8_t type);
void open_fs(fs_node_t *node, bool read, bool write);
void close_fs(fs_node_t *node);
struct dirent *readdir_fs(fs_node_t *node);
fs_node_t *finddir_fs(fs_node_t *node, char *name);
fs_node_t *get_node(char *path, fs_node_t *root);
void mount_dir(fs_node_t *mnt, fs_node_t *fs_root);
