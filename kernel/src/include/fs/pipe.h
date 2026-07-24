#pragma once

#include <types.h>
#include <stdio.h>

#define PIPE_BUF_SIZE 4096

typedef struct {
    uint8_t buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    uint32_t refcount;  /* shared by both endpoints; free buf when 0 */
} pipe_buf_t;

/* Creates a pipe pair: *read_fp and *write_fp are allocated and
 * linked to the same pipe_buf_t. Returns 0 on success. */
int pipe_create(FILE **read_fp, FILE **write_fp);
