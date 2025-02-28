#pragma once

#include <fs/vfs.h>

#define KBD_BUFFER_SZ 128
fs_node_t *stdin_node;

extern void keyboard_install();