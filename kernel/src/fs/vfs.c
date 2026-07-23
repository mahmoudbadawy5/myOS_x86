#include <fs/vfs.h>
#include <string.h>
#include <mem/malloc.h>

fs_node_t *root_dir;
fs_node_t *stdin_node;
fs_node_t *stdout_node;

// Most of the code is copied from: http://www.jamesmolloy.co.uk/tutorial_html/8.-The%20VFS%20and%20the%20initrd.html

uint32_t read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    if (node->read)
        return node->read(node, size, units, buffer);
    return 0;
}

uint32_t write_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    if (node->write)
        return node->write(node, size, units, buffer);
    return 0;
}

void seek_fs(fs_node_t *node, uint32_t offset, uint8_t type)
{
    if (node->seek)
        return node->seek(node, offset, type);
}

void open_fs(fs_node_t *node, bool read, bool write)
{
    if (node->open)
        return node->open(node);
}

void close_fs(fs_node_t *node)
{
    if (node->close)
        return node->close(node);
}

struct dirent *readdir_fs(fs_node_t *node)
{
    if ((node->flags & FS_DIRECTORY) != FS_DIRECTORY || !node->readdir)
        return NULL;
    struct dirent *d = node->readdir(node);
    mount_entry_t *m = node->mounts;
    while (m)
    {
        if (!d)
        {
            d = malloc(sizeof(struct dirent));
            d->files = NULL;
            d->file_count = 0;
        }
        char **new_files = malloc((d->file_count + 1) * sizeof(char *));
        for (uint32_t i = 0; i < d->file_count; i++)
            new_files[i] = d->files[i];
        if (d->files)
            free(d->files);
        new_files[d->file_count] = m->name;
        d->files = new_files;
        d->file_count++;
        m = m->next;
    }
    return d;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name)
{
    if ((node->flags & FS_DIRECTORY) == FS_DIRECTORY && node->finddir)
        return node->finddir(node, name);
    return NULL;
}

fs_node_t *get_node(char *path, fs_node_t *root)
{
    if (!root)
        return NULL;
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
    {
        while ((root->flags & FS_MOUNTPOINT) == FS_MOUNTPOINT && root->ptr)
            root = root->ptr;
        return root;
    }
    // Path: /[h]ello[/]worls
    //        st     en
    int st_ind = 0;
    while (path[st_ind] == '/')
        st_ind++;
    int en_ind = st_ind;
    while (path[en_ind] && path[en_ind] != '/')
        en_ind++;
    char *fname = malloc(en_ind - st_ind + 1);
    memcpy((uint8_t *)fname, (uint8_t *)path + st_ind, en_ind - st_ind);
    fname[en_ind - st_ind] = '\0';
    if ((root->flags & FS_MOUNTPOINT) == FS_MOUNTPOINT || (root->flags & FS_SYMLINK) == FS_SYMLINK)
        root = root->ptr;
    if (root->finddir)
        return get_node(path + en_ind, finddir_fs(root, fname));
    return NULL;
}

void mount_dir(fs_node_t *mnt, fs_node_t *fs_root)
{
    mnt->flags |= FS_MOUNTPOINT;
    mnt->ptr = fs_root;
}

void init_vfs(fs_node_t *root)
{
    root_dir = root;
}
