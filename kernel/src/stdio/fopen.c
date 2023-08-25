#include <fs/vfs.h>
#include <stdio.h>
#include <mem/malloc.h>

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
    int free_id = 3;
    for (int i = 3; i < MAX_FILES; i++)
    {
        if (files_open[i] == NULL)
        {
            free_id = i;
            break;
        }
    }
    if (files_open[free_id])
        free(files_open[free_id]);
    files_open[free_id] = malloc(sizeof(FILE));
    files_open[free_id]->file = get_node(path, root_dir);
    files_open[free_id]->flags = modes_mask;
    return free_id;
}