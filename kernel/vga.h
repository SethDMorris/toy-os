#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GRAY    = 7,
    VGA_DARK_GRAY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
};

#define VGA_COLOR(fg, bg) (((bg) << 4) | (fg))

void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char *s);
void vga_print_color(const char *s, uint8_t color);
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_get_color(void);

#endif
