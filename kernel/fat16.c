#include "fat16.h"
#include "ide.h"
#include "string.h"

typedef struct {
    int      valid;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t first_fat_sector;
    uint32_t first_root_sector;
    uint32_t root_sector_count;
    uint32_t first_data_sector;
    uint32_t bytes_per_cluster;
} fat16_vol_t;

static fat16_vol_t V;

void fat16_umount(void) {
    memset(&V, 0, sizeof(V));
}

int fat16_mounted(void) {
    return V.valid;
}

int fat16_mount(void) {
    uint8_t sec[512];

    fat16_umount();
    if (ide_read_sector(0, sec) != 0)
        return -1;

    uint16_t sig = (uint16_t)sec[510] | ((uint16_t)sec[511] << 8);
    if (sig != 0xAA55)
        return -2;

    uint16_t bps = (uint16_t)sec[11] | ((uint16_t)sec[12] << 8);
    if (bps != 512)
        return -3;

    uint8_t  spc   = sec[13];
    uint16_t rsv   = (uint16_t)sec[14] | ((uint16_t)sec[15] << 8);
    uint8_t  nf    = sec[16];
    uint16_t rent  = (uint16_t)sec[17] | ((uint16_t)sec[18] << 8);
    uint16_t ts16  = (uint16_t)sec[19] | ((uint16_t)sec[20] << 8);
    uint16_t spf   = (uint16_t)sec[22] | ((uint16_t)sec[23] << 8);

    if (rent == 0 || spf == 0 || spc == 0)
        return -4;

    uint32_t ts = ts16;
    if (ts16 == 0)
        ts = (uint32_t)sec[32] | ((uint32_t)sec[33] << 8) |
             ((uint32_t)sec[34] << 16) | ((uint32_t)sec[35] << 24);

    uint32_t root_sec_cnt = (rent * 32u + bps - 1u) / bps;
    uint32_t fat0         = rsv;
    uint32_t root0        = rsv + (uint32_t)nf * spf;
    uint32_t data0        = root0 + root_sec_cnt;

    V.valid               = 1;
    V.bytes_per_sector    = bps;
    V.sectors_per_cluster = spc;
    V.reserved_sectors    = rsv;
    V.num_fats            = nf;
    V.root_entry_count    = rent;
    V.sectors_per_fat     = spf;
    V.total_sectors       = ts;
    V.first_fat_sector    = fat0;
    V.first_root_sector   = root0;
    V.root_sector_count   = root_sec_cnt;
    V.first_data_sector   = data0;
    V.bytes_per_cluster   = (uint32_t)spc * bps;
    return 0;
}

static int read_sector_u32(uint32_t lba, uint8_t *buf) {
    return ide_read_sector(lba, buf);
}

static uint16_t fat16_get_next(uint16_t cluster) {
    uint32_t off   = (uint32_t)cluster * 2u;
    uint32_t sec   = V.first_fat_sector + off / 512u;
    uint32_t idx   = off % 512u;
    static uint8_t buf[512];

    if (read_sector_u32(sec, buf) != 0)
        return 0xFFF7;
    return (uint16_t)buf[idx] | ((uint16_t)buf[idx + 1] << 8);
}

/* Normalize "README.TXT" or "readme.txt" to 11-byte 8.3 uppercase, no dot in storage. */
static void to83(const char *in, char out11[12]) {
    int i, j;
    for (i = 0; i < 11; i++)
        out11[i] = ' ';
    out11[11] = '\0';

    for (i = 0; in[i] && in[i] != '.'; i++) {
        if (i >= 8)
            break;
        char c = in[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        out11[i] = c;
    }
    if (in[i] == '.') {
        i++;
        for (j = 0; j < 3 && in[i + j]; j++) {
            char c = in[i + j];
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 'a' + 'A');
            out11[8 + j] = c;
        }
    }
}

static int name83_eq(const uint8_t *ent, const char *want83) {
    char w[12];
    to83(want83, w);
    return memcmp(ent, w, 11) == 0;
}

void fat16_list_root(fat16_dirent_cb cb, void *ctx) {
    static uint8_t sec[512];

    if (!V.valid || !cb)
        return;

    for (uint32_t rs = 0; rs < V.root_sector_count; rs++) {
        if (read_sector_u32(V.first_root_sector + rs, sec) != 0)
            return;
        for (int e = 0; e < 16; e++) {
            uint8_t *ent = sec + e * 32;
            if (ent[0] == 0)
                return;
            if (ent[0] == 0xE5)
                continue;
            if (ent[11] == 0x0F)
                continue; /* LFN */
            if (ent[11] & 0x08)
                continue; /* volume label */

            char name[13];
            int ni = 0;
            for (int k = 0; k < 8 && ent[k] != ' '; k++)
                name[ni++] = (char)ent[k];
            if (ent[8] != ' ' || ent[9] != ' ' || ent[10] != ' ') {
                name[ni++] = '.';
                for (int k = 8; k < 11 && ent[k] != ' '; k++)
                    name[ni++] = (char)ent[k];
            }
            name[ni] = '\0';

            uint32_t sz = (uint32_t)ent[28] | ((uint32_t)ent[29] << 8) |
                          ((uint32_t)ent[30] << 16) | ((uint32_t)ent[31] << 24);
            cb(name, sz, ctx);
        }
    }
}

int fat16_read_file(const char *name83, void *buf, size_t max) {
    static uint8_t sec[512];

    if (!V.valid)
        return -1;

    uint16_t start_cl = 0;
    uint32_t file_sz  = 0;
    int      found    = 0;

    for (uint32_t rs = 0; rs < V.root_sector_count && !found; rs++) {
        if (read_sector_u32(V.first_root_sector + rs, sec) != 0)
            return -2;
        for (int e = 0; e < 16; e++) {
            uint8_t *ent = sec + e * 32;
            if (ent[0] == 0)
                goto done_search;
            if (ent[0] == 0xE5 || ent[11] == 0x0F || (ent[11] & 0x08))
                continue;
            if (!name83_eq(ent, name83))
                continue;
            start_cl = (uint16_t)ent[26] | ((uint16_t)ent[27] << 8);
            file_sz = (uint32_t)ent[28] | ((uint32_t)ent[29] << 8) |
                      ((uint32_t)ent[30] << 16) | ((uint32_t)ent[31] << 24);
            found = 1;
            break;
        }
    }
done_search:
    if (!found)
        return -3;
    if (start_cl < 2)
        return -4;
    if (file_sz == 0)
        return 0;

    size_t out = 0;
    uint16_t cl = start_cl;

    while (out < max) {
        uint32_t lba = V.first_data_sector +
                       ((uint32_t)(cl - 2u) * (uint32_t)V.sectors_per_cluster);

        for (uint8_t s = 0; s < V.sectors_per_cluster; s++) {
            if (read_sector_u32(lba + s, sec) != 0)
                return -5;
            size_t chunk = 512;
            if (out + chunk > max)
                chunk = max - out;
            if (out + chunk > file_sz)
                chunk = file_sz - out;
            memcpy((uint8_t *)buf + out, sec, chunk);
            out += chunk;
            if (out >= file_sz)
                return (int)out;
        }

        uint16_t nx = fat16_get_next(cl);
        if (nx >= 0xFFF8)
            break;
        if (nx < 2 || nx == cl)
            return -6;
        cl = nx;
    }

    return (int)out;
}
