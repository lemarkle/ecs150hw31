#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "disk.h"
#include "fs.h"

#define FS_SIGNATURE     "ECS150FS"
#define FS_SIGNATURE_LEN 8
#define FAT_EOC          0xFFFF
#define FS_MAX_FILES     128
#define FS_MAX_FILENAME  16

#pragma pack(push, 1)
typedef struct {
    char     signature[FS_SIGNATURE_LEN];
    uint16_t total_blocks;
    uint16_t root_dir_index;
    uint16_t data_start_index;
    uint16_t data_block_count;
    uint8_t  fat_block_count;
    uint8_t  _unused[4096 - 8 - 2*4 - 1];
} superblock_t;

typedef struct {
    char     filename[FS_MAX_FILENAME];
    uint32_t size;
    uint16_t first_data_index;
    uint8_t  _unused[10];
} dir_entry_t;
#pragma pack(pop)

static bool          g_mounted = false;
static superblock_t  g_sb;
static uint16_t     *g_fat = NULL;
static dir_entry_t   g_root[FS_MAX_FILES];

#define DATA_BLK_ABS(i) ((g_sb).data_start_index + (i))

static int read_fat(void)
{
    size_t fat_bytes = (size_t)g_sb.data_block_count * sizeof(uint16_t);
    g_fat = malloc(fat_bytes);
    if (!g_fat)
        return -1;

    uint8_t block[BLOCK_SIZE];
    size_t copied = 0;
    for (uint8_t b = 0; b < g_sb.fat_block_count; ++b) {
        if (block_read(1 + b, block) < 0)
            return -1;
        size_t to_cp = (fat_bytes - copied < BLOCK_SIZE) ? (fat_bytes - copied) : BLOCK_SIZE;
        memcpy((uint8_t *)g_fat + copied, block, to_cp);
        copied += to_cp;
    }
    return 0;
}

static int read_root_dir(void)
{
    if (block_read(g_sb.root_dir_index, g_root) < 0)
        return -1;
    return 0;
}

static void unload_metadata(void)
{
    free(g_fat);
    g_fat = NULL;
    memset(&g_sb, 0, sizeof(g_sb));
    memset(g_root, 0, sizeof(g_root));
    g_mounted = false;
}

int fs_mount(const char *diskname)
{
    if (g_mounted || !diskname)
        return -1;

    if (block_disk_open(diskname) < 0)
        return -1;

    if (block_read(0, &g_sb) < 0)
        goto fail;

    if (memcmp(g_sb.signature, FS_SIGNATURE, FS_SIGNATURE_LEN) != 0)
        goto fail;

    uint16_t computed_total = 1 + g_sb.fat_block_count + 1 + g_sb.data_block_count;
    if (computed_total != g_sb.total_blocks)
        goto fail;

    int disk_blocks = block_disk_count();
    if (disk_blocks != g_sb.total_blocks)
        goto fail;

    if (read_fat() < 0)
        goto fail;
    if (read_root_dir() < 0)
        goto fail;

    g_mounted = true;
    return 0;

fail:
    unload_metadata();
    block_disk_close();
    return -1;
}

int fs_umount(void)
{
    if (!g_mounted)
        return -1;

    unload_metadata();

    if (block_disk_close() < 0)
        return -1;

    return 0;
}

int fs_info(void)
{
    if (!g_mounted)
        return -1;

    size_t free_fat = 0;
    for (uint32_t i = 1; i < g_sb.data_block_count; ++i)
        if (g_fat[i] == 0)
            ++free_fat;

    size_t free_rdir = 0;
    for (int i = 0; i < FS_MAX_FILES; ++i)
        if (g_root[i].filename[0] == '\0')
            ++free_rdir;

    printf("FS info:\n");
    printf("total_blk_count=%u\n",   g_sb.total_blocks);
    printf("fat_blk_count=%u\n",     g_sb.fat_block_count);
    printf("rdir_blk=%u\n",          g_sb.root_dir_index);
    printf("data_blk=%u\n",          g_sb.data_start_index);
    printf("data_blk_count=%u\n",    g_sb.data_block_count);
    printf("fat_free_ratio=%zu/%u\n", free_fat, g_sb.data_block_count);
    printf("rdir_free_ratio=%zu/%d\n", free_rdir, FS_MAX_FILES);
    return 0;
}

int   fs_create(const char *filename)                { (void)filename; return -1; }
int   fs_delete(const char *filename)                { (void)filename; return -1; }
int   fs_ls(void)                                    { return -1; }
int   fs_open(const char *filename)                  { (void)filename; return -1; }
int   fs_close(int fd)                               { (void)fd;       return -1; }
int   fs_stat(int fd)                                { (void)fd;       return -1; }
int   fs_lseek(int fd, size_t offset)                { (void)fd; (void)offset; return -1; }
int   fs_write(int fd, void *buf, size_t count)      { (void)fd; (void)buf; (void)count; return -1; }
int   fs_read(int fd, void *buf, size_t count)       { (void)fd; (void)buf; (void)count; return -1; }
