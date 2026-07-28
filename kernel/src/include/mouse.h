#pragma once

#include <fs/vfs.h>

#define MOUSE_BUFFER_SZ 256

extern void mouse_install(void);
extern fs_node_t *mouse_get_node(void);
