#include <fs/vfs.h>
#include <fs/fat12.h>
#include <drivers/ata.h>
#include <mem/malloc.h>
#include <stdio.h>
#include <string.h>

#define FAT12_ROOT_INODE 0xFFFFFFFF

static fat12_fs_t fat12;
static fs_node_t fat12_root;
static void fat12_update_dir_entry(fs_node_t *file_node);

static fs_node_t *fat12_create_file(fs_node_t *dir_node, const char *name);
static int fat12_mkdir(fs_node_t *dir_node, const char *name);
static int fat12_unlink(fs_node_t *dir_node, const char *name);

static int strcasecmp_fat(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return *a - *b;
}

static int fat12_entry_to_name(fat12_dir_entry_t *entry, char *out)
{
    int j = 0;
    for (int k = 0; k < 8 && entry->name[k] != ' '; k++)
        out[j++] = entry->name[k];
    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (int k = 0; k < 3 && entry->ext[k] != ' '; k++)
            out[j++] = entry->ext[k];
    }
    out[j] = '\0';
    return j;
}

static uint16_t fat12_get_fat_entry(uint32_t cluster)
{
    uint32_t offset = cluster + (cluster / 2);
    uint16_t fat_entry;

    if (cluster & 1) {
        fat_entry = (fat12.fat_cache[offset] >> 4)
                  | ((uint16_t)fat12.fat_cache[offset + 1] << 4);
    } else {
        fat_entry = fat12.fat_cache[offset]
                  | ((uint16_t)(fat12.fat_cache[offset + 1] & 0x0F) << 8);
    }

    if (fat_entry >= 0xFF8)
        return 0xFFFF;
    if (fat_entry == 0xFF7)
        return 0xFFFF;
    return fat_entry;
}

static void fat12_set_fat_entry(uint32_t cluster, uint16_t value)
{
    uint32_t offset = cluster + (cluster / 2);

    if (cluster & 1) {
        fat12.fat_cache[offset] = (fat12.fat_cache[offset] & 0x0F) | ((value & 0x0F) << 4);
        fat12.fat_cache[offset + 1] = (uint8_t)(value >> 4);
    } else {
        fat12.fat_cache[offset] = (uint8_t)(value & 0xFF);
        fat12.fat_cache[offset + 1] = (fat12.fat_cache[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
}

static void fat12_flush_fat(void)
{
    for (uint32_t i = 0; i < fat12.bpb.num_fats; i++) {
        uint32_t fat_lba = fat12.fat_lba + (i * fat12.bpb.fat_size_sectors);
        ata_write_sectors(fat_lba, fat12.bpb.fat_size_sectors, fat12.fat_cache);
    }
}

static uint32_t fat12_allocate_cluster(void)
{
    for (uint32_t i = 2; i < fat12.total_clusters + 2; i++) {
        if (fat12_get_fat_entry(i) == 0) {
            fat12_set_fat_entry(i, 0xFFF);
            return i;
        }
    }
    return 0;
}

static void fat12_free_cluster_chain(uint32_t cluster)
{
    while (cluster != 0xFFFF && cluster >= 2) {
        uint32_t next = fat12_get_fat_entry(cluster);
        fat12_set_fat_entry(cluster, 0);
        cluster = next;
    }
}

static uint32_t fat12_cluster_to_lba(uint32_t cluster)
{
    return fat12.data_lba + (cluster - 2) * fat12.bpb.sectors_per_cluster;
}

static uint32_t fat12_read_cluster(uint32_t cluster, uint8_t *buffer)
{
    uint32_t lba = fat12_cluster_to_lba(cluster);
    return ata_read_sectors(lba, fat12.bpb.sectors_per_cluster, buffer);
}

static uint32_t fat12_write_cluster(uint32_t cluster, const uint8_t *buffer)
{
    uint32_t lba = fat12_cluster_to_lba(cluster);
    return ata_write_sectors(lba, fat12.bpb.sectors_per_cluster, buffer);
}

static uint32_t fat12_read_chain(uint32_t first_cluster, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;
    uint32_t bytes_read = 0;
    uint32_t current = first_cluster;
    uint8_t *cluster_buf = malloc(bytes_per_cluster);

    while (current != 0xFFFF && bytes_read < offset + size) {
        fat12_read_cluster(current, cluster_buf);

        if (bytes_read + bytes_per_cluster > offset && bytes_read < offset + size) {
            uint32_t start = (offset > bytes_read) ? (offset - bytes_read) : 0;
            uint32_t end = bytes_per_cluster;
            if (offset + size < bytes_read + bytes_per_cluster)
                end = offset + size - bytes_read;

            uint32_t len = end - start;
            if (len > 0 && buffer != NULL) {
                memcpy(buffer + (bytes_read > offset ? bytes_read - offset : 0),
                       cluster_buf + start, len);
            }
        }

        bytes_read += bytes_per_cluster;
        current = fat12_get_fat_entry(current);
    }

    free(cluster_buf);

    uint32_t actual = 0;
    if (bytes_read > offset)
        actual = bytes_read - offset;
    if (actual > size)
        actual = size;
    return actual;
}

static uint32_t fat12_write_chain(uint32_t first_cluster, uint32_t offset, uint32_t size, const uint8_t *buffer)
{
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;
    uint32_t total_end = offset + size;
    uint32_t bytes_written = 0;
    uint32_t current = first_cluster;
    uint8_t *cluster_buf = malloc(bytes_per_cluster);

    while (current != 0xFFFF && bytes_written < total_end) {
        uint32_t next = fat12_get_fat_entry(current);

        uint32_t clust_start = bytes_written;
        uint32_t clust_end = bytes_written + bytes_per_cluster;

        if (clust_end > offset && clust_start < total_end) {
            if (next == 0xFFFF && clust_end < total_end) {
                uint32_t new_cluster = fat12_allocate_cluster();
                if (new_cluster == 0) {
                    free(cluster_buf);
                    return bytes_written > offset ? bytes_written - offset : 0;
                }
                fat12_set_fat_entry(current, new_cluster);
                next = new_cluster;
            }

            fat12_read_cluster(current, cluster_buf);

            uint32_t start = (offset > clust_start) ? (offset - clust_start) : 0;
            uint32_t end = bytes_per_cluster;
            if (total_end < clust_end)
                end = total_end - clust_start;

            uint32_t len = end - start;
            if (len > 0 && buffer != NULL) {
                uint32_t buf_offset = clust_start > offset ? clust_start - offset : 0;
                memcpy(cluster_buf + start, buffer + buf_offset, len);
            }

            fat12_write_cluster(current, cluster_buf);
        }

        bytes_written += bytes_per_cluster;
        if (next == 0xFFFF && bytes_written < total_end) {
            uint32_t new_cluster = fat12_allocate_cluster();
            if (new_cluster == 0)
                break;
            fat12_set_fat_entry(current, new_cluster);
            next = new_cluster;
        }
        current = next;
    }

    free(cluster_buf);
    uint32_t actual = 0;
    if (total_end > offset)
        actual = (bytes_written > total_end) ? size : (bytes_written > offset ? bytes_written - offset : 0);
    return actual;
}

static uint32_t fat12_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    uint32_t total = size * units;
    uint32_t read = fat12_read_chain(node->inode, node->seek_offset, total, buffer);
    node->seek_offset += read;
    return read;
}

static uint32_t fat12_write_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    uint32_t total = size * units;
    if (total == 0)
        return 0;

    if (node->inode == 0) {
        uint32_t cluster = fat12_allocate_cluster();
        if (cluster == 0)
            return 0;
        node->inode = cluster;
    }

    uint32_t written = fat12_write_chain(node->inode, node->seek_offset, total, buffer);
    node->seek_offset += written;

    if (node->seek_offset > node->size)
        node->size = node->seek_offset;

    fat12_update_dir_entry(node);
    fat12_sync();

    return written;
}

struct update_ctx {
    const char *name;
    uint32_t first_cluster;
    uint32_t file_size;
};

static void fat12_update_dir_entry(fs_node_t *file_node)
{
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;
    struct update_ctx ctx;
    ctx.name = file_node->name;
    ctx.first_cluster = file_node->inode;
    ctx.file_size = file_node->size;

    if (fat12_root.inode == FAT12_ROOT_INODE) {
        uint32_t root_dir_sectors = ((fat12.bpb.root_entry_count * 32) + (fat12.bpb.bytes_per_sector - 1))
                                  / fat12.bpb.bytes_per_sector;
        uint32_t entries_per_sector = fat12.bpb.bytes_per_sector / sizeof(fat12_dir_entry_t);
        uint8_t *sector_buf = malloc(ATA_SECTOR_SIZE);

        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = fat12.root_lba + s;
            ata_read_sectors(lba, 1, sector_buf);
            fat12_dir_entry_t *entries = (fat12_dir_entry_t *)sector_buf;
            bool found = false;

            for (uint32_t i = 0; i < entries_per_sector; i++) {
                if (entries[i].name[0] == 0x00)
                    break;
                if ((uint8_t)entries[i].name[0] == 0xE5)
                    continue;
                if (entries[i].attributes == 0x0F)
                    continue;

                char entry_name[13];
                fat12_entry_to_name(&entries[i], entry_name);

                if (strcasecmp_fat(entry_name, ctx.name) == 0) {
                    entries[i].first_cluster = ctx.first_cluster;
                    entries[i].file_size = ctx.file_size;
                    ata_write_sectors(lba, 1, sector_buf);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        free(sector_buf);
        return;
    }

    uint8_t *cluster_buf = malloc(bytes_per_cluster);
    uint32_t cluster = fat12_root.inode;

    while (cluster != 0xFFFF) {
        uint32_t lba = fat12_cluster_to_lba(cluster);
        ata_read_sectors(lba, fat12.bpb.sectors_per_cluster, cluster_buf);
        fat12_dir_entry_t *entries = (fat12_dir_entry_t *)cluster_buf;
        uint32_t entries_per_cluster = bytes_per_cluster / sizeof(fat12_dir_entry_t);
        bool found = false;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00)
                break;
            if ((uint8_t)entries[i].name[0] == 0xE5)
                continue;
            if (entries[i].attributes == 0x0F)
                continue;

            char entry_name[13];
            fat12_entry_to_name(&entries[i], entry_name);

            if (strcasecmp_fat(entry_name, ctx.name) == 0) {
                entries[i].first_cluster = ctx.first_cluster;
                entries[i].file_size = ctx.file_size;
                ata_write_sectors(lba, fat12.bpb.sectors_per_cluster, cluster_buf);
                found = true;
                break;
            }
        }
        if (found) break;
        cluster = fat12_get_fat_entry(cluster);
    }
    free(cluster_buf);
}

static void fat12_seek_fs(fs_node_t *node, uint32_t offset, uint8_t type)
{
    if (type == SEEK_START)
        node->seek_offset = offset;
    else if (type == SEEK_CUR)
        node->seek_offset += offset;
    else
        node->seek_offset = node->size - offset;
}

static void fat12_close_fs(fs_node_t *node)
{
    (void)node;
}

static void fat12_name_to_83(const char *name, char *out_name, char *out_ext)
{
    memset((uint8_t *)out_name, ' ', 8);
    memset((uint8_t *)out_ext, ' ', 3);

    int i = 0;
    while (name[i] && name[i] != '.' && i < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out_name[i] = c;
        i++;
    }

    if (name[i] == '.') {
        i++;
        int j = 0;
        while (name[i] && j < 3) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out_ext[j] = c;
            i++;
            j++;
        }
    }
}

static fs_node_t *fat12_make_node(fat12_dir_entry_t *entry);
static void fat12_iterate_dir_write(fs_node_t *node,
    void (*callback)(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx), void *ctx);

static void fat12_iterate_dir(fs_node_t *node,
    void (*callback)(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx), void *ctx)
{
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;

    if (node->inode == FAT12_ROOT_INODE) {
        uint32_t root_dir_sectors = ((fat12.bpb.root_entry_count * 32) + (fat12.bpb.bytes_per_sector - 1))
                                  / fat12.bpb.bytes_per_sector;
        uint32_t entries_per_sector = fat12.bpb.bytes_per_sector / sizeof(fat12_dir_entry_t);
        uint8_t *sector_buf = malloc(ATA_SECTOR_SIZE);

        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            ata_read_sectors(fat12.root_lba + s, 1, sector_buf);
            fat12_dir_entry_t *entries = (fat12_dir_entry_t *)sector_buf;

            for (uint32_t i = 0; i < entries_per_sector; i++) {
                if (entries[i].name[0] == 0x00) {
                    free(sector_buf);
                    return;
                }
                if ((uint8_t)entries[i].name[0] == 0xE5)
                    continue;
                if (entries[i].attributes == 0x0F)
                    continue;
                callback(node, &entries[i], ctx);
            }
        }
        free(sector_buf);
        return;
    }

    uint8_t *cluster_buf = malloc(bytes_per_cluster);
    uint32_t cluster = node->inode;

    while (cluster != 0xFFFF) {
        uint32_t lba = fat12_cluster_to_lba(cluster);
        ata_read_sectors(lba, fat12.bpb.sectors_per_cluster, cluster_buf);

        fat12_dir_entry_t *entries = (fat12_dir_entry_t *)cluster_buf;
        uint32_t entries_per_cluster = bytes_per_cluster / sizeof(fat12_dir_entry_t);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                free(cluster_buf);
                return;
            }
            if ((uint8_t)entries[i].name[0] == 0xE5)
                continue;
            if (entries[i].attributes == 0x0F)
                continue;
            callback(node, &entries[i], ctx);
        }

        cluster = fat12_get_fat_entry(cluster);
    }

    free(cluster_buf);
}

static void fat12_iterate_dir_write(fs_node_t *node,
    void (*callback)(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx), void *ctx)
{
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;

    if (node->inode == FAT12_ROOT_INODE) {
        uint32_t root_dir_sectors = ((fat12.bpb.root_entry_count * 32) + (fat12.bpb.bytes_per_sector - 1))
                                  / fat12.bpb.bytes_per_sector;
        uint32_t entries_per_sector = fat12.bpb.bytes_per_sector / sizeof(fat12_dir_entry_t);
        uint8_t *sector_buf = malloc(ATA_SECTOR_SIZE);

        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = fat12.root_lba + s;
            ata_read_sectors(lba, 1, sector_buf);
            fat12_dir_entry_t *entries = (fat12_dir_entry_t *)sector_buf;
            bool modified = false;

        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] == 0x00) {
                callback(node, &entries[i], ctx);
                modified = true;
                break;
            }
            if ((uint8_t)entries[i].name[0] == 0xE5) {
                callback(node, &entries[i], ctx);
                modified = true;
                break;
            }
        }

            if (modified) {
                ata_write_sectors(lba, 1, sector_buf);
            }
        }
        free(sector_buf);
        return;
    }

    uint8_t *cluster_buf = malloc(bytes_per_cluster);
    uint32_t cluster = node->inode;

    while (cluster != 0xFFFF) {
        uint32_t lba = fat12_cluster_to_lba(cluster);
        ata_read_sectors(lba, fat12.bpb.sectors_per_cluster, cluster_buf);

        fat12_dir_entry_t *entries = (fat12_dir_entry_t *)cluster_buf;
        uint32_t entries_per_cluster = bytes_per_cluster / sizeof(fat12_dir_entry_t);
        bool modified = false;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                callback(node, &entries[i], ctx);
                modified = true;
                break;
            }
        }

        if (modified) {
            ata_write_sectors(lba, fat12.bpb.sectors_per_cluster, cluster_buf);
        }

        cluster = fat12_get_fat_entry(cluster);
    }

    free(cluster_buf);
}

struct readdir_ctx {
    dirent_t *dire;
    uint32_t count;
};

static void readdir_callback(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx)
{
    (void)node;
    struct readdir_ctx *c = (struct readdir_ctx *)ctx;

    char name[13];
    int j = fat12_entry_to_name(entry, name);

    c->dire->files[c->count] = malloc(j + 1);
    strcpy(c->dire->files[c->count], name);
    c->count++;
}

dirent_t *fat12_readdir_fs(fs_node_t *node)
{
    if (!(node->flags & FS_DIRECTORY))
        return NULL;

    dirent_t *dire = malloc(sizeof(dirent_t));
    dire->file_count = 0;
    dire->files = malloc(sizeof(char *) * 128);

    struct readdir_ctx ctx;
    ctx.dire = dire;
    ctx.count = 0;

    fat12_iterate_dir(node, readdir_callback, &ctx);
    dire->file_count = ctx.count;

    return dire;
}

struct finddir_ctx {
    const char *name;
    fs_node_t *result;
};

static void finddir_callback(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx)
{
    (void)node;
    struct finddir_ctx *c = (struct finddir_ctx *)ctx;
    if (c->result)
        return;

    char entry_name[13];
    fat12_entry_to_name(entry, entry_name);

    if (strcasecmp_fat(entry_name, c->name) == 0) {
        c->result = fat12_make_node(entry);
    }
}

fs_node_t *fat12_finddir_fs(fs_node_t *node, char *name)
{
    if (!(node->flags & FS_DIRECTORY))
        return NULL;

    struct finddir_ctx ctx;
    ctx.name = name;
    ctx.result = NULL;

    fat12_iterate_dir(node, finddir_callback, &ctx);
    return ctx.result;
}

static fs_node_t *fat12_make_node(fat12_dir_entry_t *entry)
{
    fs_node_t *node = malloc(sizeof(fs_node_t));
    memset((uint8_t *)node, 0, sizeof(fs_node_t));

    char name[13];
    fat12_entry_to_name(entry, name);

    strcpy(node->name, name);
    node->size = entry->file_size;
    node->inode = entry->first_cluster;

    if (entry->attributes & 0x10) {
        node->flags = FS_DIRECTORY;
        node->readdir = fat12_readdir_fs;
        node->finddir = fat12_finddir_fs;
        node->create = fat12_create_file;
        node->mkdir = fat12_mkdir;
        node->unlink = fat12_unlink;
    } else {
        node->flags = FS_FILE;
        node->read = fat12_read_fs;
        node->write = fat12_write_fs;
        node->seek = fat12_seek_fs;
        node->close = fat12_close_fs;
    }

    return node;
}

fs_node_t *fat12_get_root(void)
{
    return &fat12_root;
}

struct create_file_ctx {
    const char *name;
    fs_node_t *result;
};

static void create_file_callback(fs_node_t *node, fat12_dir_entry_t *entry, void *ctx)
{
    (void)node;
    struct create_file_ctx *c = (struct create_file_ctx *)ctx;
    if (c->result)
        return;

    char entry_name[8], entry_ext[3];
    fat12_name_to_83(c->name, entry_name, entry_ext);

    uint32_t cluster = fat12_allocate_cluster();

    memset((uint8_t *)entry, 0, sizeof(fat12_dir_entry_t));
    memcpy((uint8_t *)entry->name, (uint8_t *)entry_name, 8);
    memcpy((uint8_t *)entry->ext, (uint8_t *)entry_ext, 3);
    entry->attributes = 0x20;
    entry->first_cluster = cluster;
    entry->file_size = 0;

    c->result = fat12_make_node(entry);
}

fs_node_t *fat12_create_file(fs_node_t *dir_node, const char *name)
{
    struct finddir_ctx fctx;
    fctx.name = name;
    fctx.result = NULL;
    fat12_iterate_dir(dir_node, finddir_callback, &fctx);
    if (fctx.result)
        return fctx.result;

    struct create_file_ctx ctx;
    ctx.name = name;
    ctx.result = NULL;

    fat12_iterate_dir_write(dir_node, create_file_callback, &ctx);
    return ctx.result;
}

fs_node_t *fat12_create(const char *name)
{
    fs_node_t *file = fat12_create_file(&fat12_root, name);
    if (file) {
        fat12_flush_fat();
    }
    return file;
}

int fat12_mkdir(fs_node_t *dir_node, const char *name)
{
    if (!name || !*name)
        return -1;

    /* Check if already exists */
    struct finddir_ctx fctx;
    fctx.name = name;
    fctx.result = NULL;
    fat12_iterate_dir(dir_node, finddir_callback, &fctx);
    if (fctx.result) {
        free(fctx.result);
        return -1;
    }

    uint32_t cluster = fat12_allocate_cluster();
    if (cluster == 0)
        return -1;

    /* Initialize the cluster with . and .. entries */
    uint32_t bytes_per_cluster = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;
    uint8_t *cluster_buf = malloc(bytes_per_cluster);
    memset(cluster_buf, 0, bytes_per_cluster);

    fat12_dir_entry_t *entries = (fat12_dir_entry_t *)cluster_buf;

    /* Entry 0: "." — current directory */
    memset(entries[0].name, ' ', 8);
    memset(entries[0].ext, ' ', 3);
    entries[0].name[0] = '.';
    entries[0].attributes = 0x10;
    entries[0].first_cluster = cluster;

    /* Entry 1: ".." — parent directory */
    memset(entries[1].name, ' ', 8);
    memset(entries[1].ext, ' ', 3);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attributes = 0x10;
    if (dir_node->inode == FAT12_ROOT_INODE)
        entries[1].first_cluster = 0;
    else
        entries[1].first_cluster = dir_node->inode;

    fat12_write_cluster(cluster, cluster_buf);
    free(cluster_buf);

    /* Write directory entry in parent using a dedicated callback */
    struct mkdir_entry_ctx {
        const char *name;
        uint32_t cluster;
        bool done;
    };

    void mkdir_entry_cb(fs_node_t *node, fat12_dir_entry_t *entry, void *vctx) {
        (void)node;
        struct mkdir_entry_ctx *c = (struct mkdir_entry_ctx *)vctx;
        if (c->done) return;

        char entry_name_8[8], entry_ext_3[3];
        fat12_name_to_83(c->name, entry_name_8, entry_ext_3);

        memset((uint8_t *)entry, 0, sizeof(fat12_dir_entry_t));
        memcpy((uint8_t *)entry->name, (uint8_t *)entry_name_8, 8);
        memcpy((uint8_t *)entry->ext, (uint8_t *)entry_ext_3, 3);
        entry->attributes = 0x10;
        entry->first_cluster = c->cluster;
        entry->file_size = 0;
        c->done = true;
    }

    struct mkdir_entry_ctx mctx;
    mctx.name = name;
    mctx.cluster = cluster;
    mctx.done = false;

    fat12_iterate_dir_write(dir_node, mkdir_entry_cb, &mctx);

    if (!mctx.done) {
        /* Failed to write entry — free the cluster */
        fat12_free_cluster_chain(cluster);
        return -1;
    }

    fat12_flush_fat();
    return 0;
}

int fat12_unlink(fs_node_t *dir_node, const char *name)
{
    if (!name || !*name)
        return -1;

    /* Find the entry */
    struct finddir_ctx fctx;
    fctx.name = name;
    fctx.result = NULL;
    fat12_iterate_dir(dir_node, finddir_callback, &fctx);
    if (!fctx.result)
        return -1;

    /* Don't allow deleting directories */
    if (fctx.result->flags & FS_DIRECTORY) {
        free(fctx.result);
        return -1;
    }

    uint32_t file_cluster = fctx.result->inode;
    free(fctx.result);

    /* Mark dir entry as deleted and free cluster chain */
    uint32_t bpc = fat12.bpb.sectors_per_cluster * fat12.bpb.bytes_per_sector;
    bool found = false;

    /* Try root dir */
    if (dir_node->inode == FAT12_ROOT_INODE) {
        uint32_t root_dir_sectors = ((fat12.bpb.root_entry_count * 32) + (fat12.bpb.bytes_per_sector - 1))
                                  / fat12.bpb.bytes_per_sector;
        uint32_t entries_per_sector = fat12.bpb.bytes_per_sector / sizeof(fat12_dir_entry_t);
        uint8_t *sector_buf = malloc(ATA_SECTOR_SIZE);

        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = fat12.root_lba + s;
            ata_read_sectors(lba, 1, sector_buf);
            fat12_dir_entry_t *ents = (fat12_dir_entry_t *)sector_buf;

            for (uint32_t i = 0; i < entries_per_sector; i++) {
                if (ents[i].name[0] == 0x00)
                    break;
                if ((uint8_t)ents[i].name[0] == 0xE5) continue;
                if (ents[i].attributes == 0x0F) continue;

                char entry_name[13];
                fat12_entry_to_name(&ents[i], entry_name);
                if (strcasecmp_fat(entry_name, name) == 0) {
                    ents[i].name[0] = (char)0xE5;
                    ata_write_sectors(lba, 1, sector_buf);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        free(sector_buf);
    } else {
        uint8_t *cluster_buf = malloc(bpc);
        uint32_t cl = dir_node->inode;

        while (cl != 0xFFFF) {
            fat12_read_cluster(cl, cluster_buf);
            fat12_dir_entry_t *ents = (fat12_dir_entry_t *)cluster_buf;
            uint32_t epc = bpc / sizeof(fat12_dir_entry_t);

            for (uint32_t i = 0; i < epc; i++) {
                if (ents[i].name[0] == 0x00) break;
                if ((uint8_t)ents[i].name[0] == 0xE5) continue;
                if (ents[i].attributes == 0x0F) continue;

                char entry_name[13];
                fat12_entry_to_name(&ents[i], entry_name);
                if (strcasecmp_fat(entry_name, name) == 0) {
                    ents[i].name[0] = (char)0xE5;
                    fat12_write_cluster(cl, cluster_buf);
                    found = true;
                    break;
                }
            }
            if (found) break;
            cl = fat12_get_fat_entry(cl);
        }
        free(cluster_buf);
    }

    if (!found)
        return -1;

    /* Free cluster chain */
    if (file_cluster >= 2)
        fat12_free_cluster_chain(file_cluster);

    fat12_flush_fat();
    return 0;
}

void fat12_sync(void)
{
    fat12_flush_fat();
}

void init_fat12(uint32_t lba_offset)
{
    ata_device_t *dev = ata_get_device();
    if (!dev || !dev->present) {
        printf("[FAT12] No ATA device found\n");
        return;
    }

    fat12.lba_offset = lba_offset;

    uint8_t *sector = malloc(ATA_SECTOR_SIZE);
    if (!ata_read_sectors(lba_offset, 1, sector)) {
        printf("[FAT12] Failed to read boot sector\n");
        free(sector);
        return;
    }

    memcpy((uint8_t *)&fat12.bpb, sector, sizeof(fat12_bpb_t));

    if (fat12.bpb.bytes_per_sector != 512) {
        printf("[FAT12] Unsupported sector size: %d\n", fat12.bpb.bytes_per_sector);
        free(sector);
        return;
    }

    uint32_t root_dir_sectors = ((fat12.bpb.root_entry_count * 32) + (fat12.bpb.bytes_per_sector - 1))
                              / fat12.bpb.bytes_per_sector;

    fat12.fat_lba = lba_offset + fat12.bpb.reserved_sectors;
    fat12.root_lba = fat12.fat_lba + (fat12.bpb.num_fats * fat12.bpb.fat_size_sectors);
    fat12.data_lba = fat12.root_lba + root_dir_sectors;

    uint32_t fat_bytes = fat12.bpb.fat_size_sectors * fat12.bpb.bytes_per_sector;
    fat12.fat_cache = malloc(fat_bytes);
    if (!ata_read_sectors(fat12.fat_lba, fat12.bpb.fat_size_sectors, fat12.fat_cache)) {
        printf("[FAT12] Failed to read FAT\n");
        free(fat12.fat_cache);
        free(sector);
        return;
    }

    uint32_t data_sectors;
    if (fat12.bpb.total_sectors_16 != 0)
        data_sectors = fat12.bpb.total_sectors_16 - fat12.data_lba + lba_offset;
    else
        data_sectors = fat12.bpb.total_sectors_32 - fat12.data_lba + lba_offset;

    fat12.total_clusters = data_sectors / fat12.bpb.sectors_per_cluster;

    printf("[FAT12] Found: %d clusters, FAT at LBA %d, root at LBA %d, data at LBA %d\n",
           fat12.total_clusters, fat12.fat_lba, fat12.root_lba, fat12.data_lba);

    memset((uint8_t *)&fat12_root, 0, sizeof(fs_node_t));
    strcpy(fat12_root.name, "mnt");
    fat12_root.flags = FS_DIRECTORY;
    fat12_root.inode = FAT12_ROOT_INODE;
    fat12_root.readdir = fat12_readdir_fs;
    fat12_root.finddir = fat12_finddir_fs;
    fat12_root.create = fat12_create_file;
    fat12_root.mkdir = fat12_mkdir;
    fat12_root.unlink = fat12_unlink;

    free(sector);
}
