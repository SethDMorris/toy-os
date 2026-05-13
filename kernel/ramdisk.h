#ifndef RAMDISK_H
#define RAMDISK_H

#include <stddef.h>
#include <stdint.h>

void ramdisk_init(void);

int ramdisk_write(const char *name, const void *data, size_t len);
int ramdisk_rm(const char *name);
int ramdisk_get(const char *name, const uint8_t **out_data, size_t *out_len);

typedef void (*ramdisk_iter_fn)(const char *name, size_t size, void *ctx);
void ramdisk_foreach(ramdisk_iter_fn fn, void *ctx);

#endif
