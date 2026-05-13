#include "vga.h"
#include "ports.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)

static int cursor_x;
static int cursor_y;
static uint8_t color_attr;

static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void scroll(void) {
    if (cursor_y < VGA_HEIGHT)
        return;
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
        VGA_MEMORY[i] = (uint16_t)color_attr << 8 | ' ';
    cursor_y = VGA_HEIGHT - 1;
}

void vga_init(void) {
    color_attr = VGA_COLOR(VGA_LIGHT_GRAY, VGA_BLACK);
    cursor_x = 0;
    cursor_y = 0;
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = (uint16_t)color_attr << 8 | ' ';
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] =
                (uint16_t)color_attr << 8 | ' ';
        }
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] =
            (uint16_t)color_attr << 8 | (uint8_t)c;
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    scroll();
    update_cursor();
}

void vga_print(const char *s) {
    while (*s)
        vga_putchar(*s++);
}

void vga_print_color(const char *s, uint8_t color) {
    uint8_t saved = color_attr;
    color_attr = color;
    vga_print(s);
    color_attr = saved;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color_attr = VGA_COLOR(fg, bg);
}

uint8_t vga_get_color(void) {
    return color_attr;
}
