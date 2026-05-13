#include "minivm.h"
#include "string.h"
#include "vga.h"

#define BC_STACK  64
#define OP_HALT   0x00
#define OP_PUSH32 0x01
#define OP_ADD    0x02
#define OP_SUB    0x03
#define OP_MUL    0x04
#define OP_PRINT  0x05

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0';
}

static int emit_u32(uint8_t *out, size_t *w, size_t cap, uint32_t v) {
    if (*w + 4 > cap)
        return -1;
    out[(*w)++] = (uint8_t)(v & 0xFF);
    out[(*w)++] = (uint8_t)((v >> 8) & 0xFF);
    out[(*w)++] = (uint8_t)((v >> 16) & 0xFF);
    out[(*w)++] = (uint8_t)((v >> 24) & 0xFF);
    return 0;
}

/* Keyword at p; end is one-past-last valid char (src not necessarily '\0'-terminated). */
static int kw(const char *p, const char *end, const char *k, int klen) {
    if ((size_t)(end - p) < (size_t)klen)
        return 0;
    if (strncmp(p, k, (size_t)klen) != 0)
        return 0;
    if ((size_t)(end - p) == (size_t)klen)
        return 1;
    return is_ws(p[klen]) ? 1 : 0;
}

static int parse_int(const char **pp, int32_t *out) {
    const char *p = *pp;
    while (is_ws(*p))
        p++;
    int neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return -1;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *out = neg ? -(int32_t)v : (int32_t)v;
    *pp = p;
    return 0;
}

int minivm_run(const uint8_t *code, size_t len) {
    int32_t st[BC_STACK];
    int sp = 0;
    size_t ip = 0;

    while (ip < len) {
        uint8_t op = code[ip++];

        if (op == OP_HALT)
            return 0;

        if (op == OP_PUSH32) {
            if (ip + 4 > len)
                return -4;
            uint32_t u = (uint32_t)code[ip] | ((uint32_t)code[ip + 1] << 8) |
                          ((uint32_t)code[ip + 2] << 16) | ((uint32_t)code[ip + 3] << 24);
            ip += 4;
            if (sp >= BC_STACK)
                return -5;
            st[sp++] = (int32_t)u;
            continue;
        }

        if (op == OP_ADD || op == OP_SUB || op == OP_MUL) {
            if (sp < 2)
                return -1;
            int32_t b = st[--sp];
            int32_t a = st[--sp];
            int32_t r;
            if (op == OP_ADD)
                r = a + b;
            else if (op == OP_SUB)
                r = a - b;
            else
                r = (int32_t)((uint32_t)a * (uint32_t)b);
            if (sp >= BC_STACK)
                return -5;
            st[sp++] = r;
            continue;
        }

        if (op == OP_PRINT) {
            if (sp < 1)
                return -1;
            int32_t v = st[--sp];
            char buf[12];
            int_to_str((int)v, buf);
            vga_putchar('\n');
            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_print("  [bc] ");
            vga_print(buf);
            vga_putchar('\n');
            vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
            continue;
        }

        return -2;
    }
    return 0;
}

int minivm_compile(const char *src, size_t srclen,
                   uint8_t *out, size_t outcap, size_t *outlen) {
    const char *end = src + srclen;
    const char *p = src;
    size_t w = 0;

    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ';'))
            p++;
        if (p >= end)
            break;
        if (*p == '#') {
            while (p < end && *p != '\n')
                p++;
            continue;
        }

        if (kw(p, end, "push", 4)) {
            p += 4;
            int32_t imm;
            if (parse_int(&p, &imm) != 0)
                return -10;
            if (w + 5 > outcap)
                return -1;
            out[w++] = OP_PUSH32;
            if (emit_u32(out, &w, outcap, (uint32_t)imm) != 0)
                return -1;
            continue;
        }

        if (kw(p, end, "add", 3)) {
            if (w + 1 > outcap)
                return -1;
            out[w++] = OP_ADD;
            p += 3;
            continue;
        }
        if (kw(p, end, "sub", 3)) {
            if (w + 1 > outcap)
                return -1;
            out[w++] = OP_SUB;
            p += 3;
            continue;
        }
        if (kw(p, end, "mul", 3)) {
            if (w + 1 > outcap)
                return -1;
            out[w++] = OP_MUL;
            p += 3;
            continue;
        }
        if (kw(p, end, "print", 5)) {
            if (w + 1 > outcap)
                return -1;
            out[w++] = OP_PRINT;
            p += 5;
            continue;
        }
        if (kw(p, end, "halt", 4)) {
            if (w + 1 > outcap)
                return -1;
            out[w++] = OP_HALT;
            p += 4;
            continue;
        }

        return -11;
    }

    if (w + 1 > outcap)
        return -1;
    if (w == 0 || out[w - 1] != OP_HALT)
        out[w++] = OP_HALT;

    *outlen = w;
    return 0;
}
