#pragma once

#include <types.h>
#include <fs/vfs.h>

typedef struct __attribute__((packed)) {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat12_bpb_t;

typedef struct __attribute__((packed)) {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster;
    uint32_t file_size;
} fat12_dir_entry_t;

typedef struct {
    fat12_bpb_t bpb;
    uint32_t    fat_lba;
    uint32_t    root_lba;
    uint32_t    data_lba;
    uint8_t    *fat_cache;
    uint32_t    total_clusters;
    uint32_t    lba_offset;
} fat12_fs_t;

void init_fat12(uint32_t lba_offset);
fs_node_t *fat12_get_root(void);
fs_node_t *fat12_create(const char *name);
void fat12_sync(void);
