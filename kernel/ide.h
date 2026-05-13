#ifndef IDE_H
#define IDE_H

#include <stddef.h>
#include <stdint.h>

void ide_init(void);
int  ide_read_sector(uint32_t lba, void *buf512);
int  ide_write_sector(uint32_t lba, const void *buf512);

#endif
