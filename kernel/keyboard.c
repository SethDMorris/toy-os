#include "keyboard.h"
#include "ports.h"

#define KB_BUF_SIZE 256
#define LSHIFT_PRESS    0x2A
#define RSHIFT_PRESS    0x36
#define LSHIFT_RELEASE  0xAA
#define RSHIFT_RELEASE  0xB6

static char kb_buf[KB_BUF_SIZE];
static volatile int kb_w;
static int kb_r;
static int shift_held;

/* US QWERTY scancode set 1 */
static const char sc_normal[128] = {
     0,   27,  '1', '2', '3', '4', '5', '6',   /* 0x00 */
    '7', '8', '9', '0', '-', '=','\b','\t',     /* 0x08 */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',     /* 0x10 */
    'o', 'p', '[', ']','\n',  0,  'a', 's',      /* 0x18 */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 0x20 */
   '\'', '`',  0, '\\', 'z', 'x', 'c', 'v',     /* 0x28 */
    'b', 'n', 'm', ',', '.', '/',  0,  '*',      /* 0x30 */
     0,  ' ',  0,   0,   0,   0,   0,   0,       /* 0x38 */
};

static const char sc_shift[128] = {
     0,   27,  '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+','\b','\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}','\n',  0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?',  0,  '*',
     0,  ' ',  0,   0,   0,   0,   0,   0,
};

void keyboard_irq_handler(void) {
    uint8_t sc = inb(0x60);

    if (sc == LSHIFT_PRESS || sc == RSHIFT_PRESS) {
        shift_held = 1;
        outb(0x20, 0x20);
        return;
    }
    if (sc == LSHIFT_RELEASE || sc == RSHIFT_RELEASE) {
        shift_held = 0;
        outb(0x20, 0x20);
        return;
    }
    if (sc & 0x80) {               /* key release — ignore */
        outb(0x20, 0x20);
        return;
    }

    char c = shift_held ? sc_shift[sc] : sc_normal[sc];
    if (c) {
        kb_buf[kb_w] = c;
        kb_w = (kb_w + 1) % KB_BUF_SIZE;
    }

    outb(0x20, 0x20);              /* EOI to master PIC */
}

char keyboard_getchar(void) {
    if (kb_r == kb_w)
        return 0;
    char c = kb_buf[kb_r];
    kb_r = (kb_r + 1) % KB_BUF_SIZE;
    return c;
}
