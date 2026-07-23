#include <fs/pipe.h>
#include <mem/malloc.h>
#include <string.h>

static void pipe_close_fn(fs_node_t *node)
{
    pipe_buf_t *pb = (pipe_buf_t *)node->ptr;
    if (pb) {
        /* Mark the pointer NULL on both sides so the other side doesn't double-free.
         * Both read_node->ptr and write_node->ptr point to the same pb. */
        /* We can't reach the other node from here, so we use a trick:
         * free only if refcount is 0 (we are the last holder). */
        if (node->refcount == 0) {
            free(pb);
            node->ptr = 0;
        }
    }
}

static uint32_t pipe_read_fn(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    pipe_buf_t *pb = (pipe_buf_t *)node->ptr;
    uint32_t total = size * units;
    uint32_t read = 0;

    while (read < total && pb->count > 0) {
        buffer[read++] = pb->buf[pb->read_pos];
        pb->read_pos = (pb->read_pos + 1) % PIPE_BUF_SIZE;
        pb->count--;
    }
    return read;
}

static uint32_t pipe_write_fn(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    pipe_buf_t *pb = (pipe_buf_t *)node->ptr;
    uint32_t total = size * units;
    uint32_t written = 0;

    while (written < total && pb->count < PIPE_BUF_SIZE) {
        pb->buf[pb->write_pos] = buffer[written++];
        pb->write_pos = (pb->write_pos + 1) % PIPE_BUF_SIZE;
        pb->count++;
    }
    return written;
}

int pipe_create(FILE **read_fp, FILE **write_fp)
{
    pipe_buf_t *pb = malloc(sizeof(pipe_buf_t));
    if (!pb) return -1;
    pb->read_pos = 0;
    pb->write_pos = 0;
    pb->count = 0;

    fs_node_t *read_node = malloc(sizeof(fs_node_t));
    memset((unsigned char *)read_node, 0, sizeof(fs_node_t));
    read_node->flags = FS_PIPE;
    read_node->ptr = (fs_node_t *)pb;
    read_node->read = pipe_read_fn;
    read_node->close = pipe_close_fn;
    read_node->refcount = 1;

    fs_node_t *write_node = malloc(sizeof(fs_node_t));
    memset((unsigned char *)write_node, 0, sizeof(fs_node_t));
    write_node->flags = FS_PIPE;
    write_node->ptr = (fs_node_t *)pb;
    write_node->write = pipe_write_fn;
    write_node->close = pipe_close_fn;
    write_node->refcount = 1;

    *read_fp = malloc(sizeof(FILE));
    (*read_fp)->flags = FILE_READ;
    (*read_fp)->file = read_node;

    *write_fp = malloc(sizeof(FILE));
    (*write_fp)->flags = FILE_WRITE;
    (*write_fp)->file = write_node;

    return 0;
}
