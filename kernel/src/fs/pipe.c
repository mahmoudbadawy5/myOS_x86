#include <fs/pipe.h>
#include <mem/malloc.h>
#include <string.h>

static void pipe_close_fn(fs_node_t *node)
{
    pipe_buf_t *pb = (pipe_buf_t *)node->ptr;
    if (pb) {
        /* Both endpoints share the same pipe_buf_t.
         * Decrement the shared refcount; free the buffer only
         * when the last endpoint closes. */
        if (pb->refcount > 0)
            pb->refcount--;
        if (pb->refcount == 0) {
            free(pb);
        }
        node->ptr = 0;
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
    pb->refcount = 2; /* one for each endpoint */

    fs_node_t *read_node = malloc(sizeof(fs_node_t));
    if (!read_node) { free(pb); return -1; }
    memset((unsigned char *)read_node, 0, sizeof(fs_node_t));
    read_node->flags = FS_PIPE;
    read_node->ptr = (fs_node_t *)pb;
    read_node->read = pipe_read_fn;
    read_node->close = pipe_close_fn;
    read_node->refcount = 1;

    fs_node_t *write_node = malloc(sizeof(fs_node_t));
    if (!write_node) { free(read_node); free(pb); return -1; }
    memset((unsigned char *)write_node, 0, sizeof(fs_node_t));
    write_node->flags = FS_PIPE;
    write_node->ptr = (fs_node_t *)pb;
    write_node->write = pipe_write_fn;
    write_node->close = pipe_close_fn;
    write_node->refcount = 1;

    *read_fp = malloc(sizeof(FILE));
    if (!*read_fp) { free(write_node); free(read_node); free(pb); return -1; }
    (*read_fp)->flags = FILE_READ;
    (*read_fp)->file = read_node;

    *write_fp = malloc(sizeof(FILE));
    if (!*write_fp) { free(*read_fp); free(write_node); free(read_node); free(pb); return -1; }
    (*write_fp)->flags = FILE_WRITE;
    (*write_fp)->file = write_node;

    return 0;
}
