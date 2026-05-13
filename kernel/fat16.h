#ifndef FAT16_H
#define FAT16_H

#include <stddef.h>
#include <stdint.h>

/* Partitionless FAT16 volume starting at LBA 0 (mkfs.vfat -F 16 -C ...). */

int  fat16_mount(void);
void fat16_umount(void);
int  fat16_mounted(void);

typedef void (*fat16_dirent_cb)(const char *name83, uint32_t size, void *ctx);
void fat16_list_root(fat16_dirent_cb cb, void *ctx);

/* Read entire file by 8.3 name (e.g. "README.TXT"). Returns bytes read or negative error. */
int fat16_read_file(const char *name83, void *buf, size_t max);

#endif
