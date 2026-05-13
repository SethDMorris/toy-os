#include "ramdisk.h"
#include "kmalloc.h"
#include "string.h"

#define RAMDISK_MAX_FILES 32
#define RAMDISK_NAME_MAX  32

typedef struct {
    char     name[RAMDISK_NAME_MAX];
    uint8_t *data;
    size_t   size;
    int      used;
} ramdisk_entry_t;

static ramdisk_entry_t entries[RAMDISK_MAX_FILES];

void ramdisk_init(void) {
    memset(entries, 0, sizeof(entries));
}

static int find_slot(const char *name) {
    for (int i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (entries[i].used && strcmp(entries[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int alloc_slot(void) {
    for (int i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (!entries[i].used)
            return i;
    }
    return -1;
}

static int validate_name(const char *name) {
    if (!name || !name[0])
        return -1;
    size_t n = strlen(name);
    if (n >= RAMDISK_NAME_MAX)
        return -1;
    for (size_t i = 0; i < n; i++) {
        if (name[i] == '/' || name[i] == ' ')
            return -1;
    }
    return 0;
}

int ramdisk_write(const char *name, const void *data, size_t len) {
    if (validate_name(name) != 0)
        return -1;

    int slot = find_slot(name);
    if (slot < 0)
        slot = alloc_slot();
    if (slot < 0)
        return -2;

    void *buf = NULL;
    if (len > 0) {
        buf = kmalloc(len);
        if (!buf)
            return -3;
        memcpy(buf, data, len);
    }

    strncpy(entries[slot].name, name, RAMDISK_NAME_MAX - 1);
    entries[slot].name[RAMDISK_NAME_MAX - 1] = '\0';
    entries[slot].data = (uint8_t *)buf;
    entries[slot].size = len;
    entries[slot].used = 1;
    return 0;
}

int ramdisk_rm(const char *name) {
    if (validate_name(name) != 0)
        return -1;
    int slot = find_slot(name);
    if (slot < 0)
        return -2;
    entries[slot].used = 0;
    entries[slot].data = NULL;
    entries[slot].size = 0;
    entries[slot].name[0] = '\0';
    return 0;
}

int ramdisk_get(const char *name, const uint8_t **out_data, size_t *out_len) {
    int slot = find_slot(name);
    if (slot < 0)
        return -1;
    *out_data = entries[slot].data;
    *out_len = entries[slot].size;
    return 0;
}

void ramdisk_foreach(ramdisk_iter_fn fn, void *ctx) {
    if (!fn)
        return;
    for (int i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (entries[i].used)
            fn(entries[i].name, entries[i].size, ctx);
    }
}
