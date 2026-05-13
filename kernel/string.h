#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
int     strncmp(const char *a, const char *b, size_t n);
char   *strcpy(char *dst, const char *src);
char   *strncpy(char *dst, const char *src, size_t n);
void   *memset(void *ptr, int val, size_t n);
void   *memcpy(void *dst, const void *src, size_t n);
int     memcmp(const void *a, const void *b, size_t n);
void    int_to_str(int value, char *buf);
void    uint_to_hex(uint32_t value, char *buf);

#endif
