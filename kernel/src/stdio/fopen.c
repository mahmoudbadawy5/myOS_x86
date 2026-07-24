#include <fs/vfs.h>
#include <stdio.h>
#include <mem/malloc.h>
#include <proc/process.h>

int get_mode(char *modes)
{
    int mode = 0;
    for (int i = 0; modes[i]; i++)
    {
        if (modes[i] == 'a')
            mode |= FILE_APPEND;
        if (modes[i] == 'r')
            mode |= FILE_READ;
        if (modes[i] == 'w')
            mode |= FILE_WRITE;
    }
    return mode;
}

int fopen(char *path, char *modes)
{
    int modes_mask = get_mode(modes);

    fs_node_t *node = get_node(path, root_dir);
    if (!node && (modes_mask & (FILE_WRITE | FILE_APPEND)))
        node = create_node(path, root_dir);
    if (!node)
        return -1;

    int free_id = 3;
    for (int i = 3; i < MAX_FILES; i++)
    {
        if (current_process->files_open[i] == NULL)
        {
            free_id = i;
            break;
        }
    }
    if (current_process->files_open[free_id])
        free(current_process->files_open[free_id]);
    current_process->files_open[free_id] = malloc(sizeof(FILE));
    current_process->files_open[free_id]->file = node;
    current_process->files_open[free_id]->flags = modes_mask;
    node->refcount++;
    return free_id;
}