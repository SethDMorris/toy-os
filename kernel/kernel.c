#include "vga.h"
#include "keyboard.h"
#include "idt.h"
#include "string.h"
#include "ports.h"
#include "kmalloc.h"
#include "ramdisk.h"

#define CMD_BUF_SIZE 256

static char cmd_buf[CMD_BUF_SIZE];
static int  cmd_len;

volatile uint32_t timer_ticks;

/* ── Timer IRQ handler (called from asm stub) ──────────────────────── */

void timer_tick_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20);
}

/* ── CMOS real-time clock ──────────────────────────────────────────── */

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t v) {
    return ((v >> 4) * 10) + (v & 0x0F);
}

/* ── Simple PRNG (for fortune) ─────────────────────────────────────── */

static uint32_t rand_state = 123456789;

static uint32_t simple_rand(void) {
    rand_state ^= timer_ticks;
    rand_state ^= rand_state << 13;
    rand_state ^= rand_state >> 17;
    rand_state ^= rand_state << 5;
    return rand_state;
}

/* ── Pretty-print helpers ──────────────────────────────────────────── */

static void print_label(const char *label) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print(label);
}

static void print_value(const char *val) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(val);
}

static void print_num(int n) {
    char buf[12];
    int_to_str(n, buf);
    vga_print(buf);
}

static void print_uint32(uint32_t n) {
    char buf[12];
    int_to_str((int)n, buf);
    vga_print(buf);
}

static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r')
        s++;
    return s;
}

/* ── Banner ────────────────────────────────────────────────────────── */

static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("\n  ");
    /* top border */
    for (int i = 0; i < 56; i++) vga_putchar('=');
    vga_print("\n  ");

    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_print("                 Welcome to miniOS v0.2                 ");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("\n  ");

    /* bottom border */
    for (int i = 0; i < 56; i++) vga_putchar('=');
    vga_print("\n\n");

    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print("  A toy operating system built from scratch in C & x86 asm.\n");
    vga_print("  Type ");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("help");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print(" for available commands.\n\n");
}

/* ── Prompt ────────────────────────────────────────────────────────── */

static void print_prompt(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("miniOS");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print("> ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* ── Shell commands ────────────────────────────────────────────────── */

static void cmd_help(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("\n  Available Commands:\n\n");

    print_label("  help     ");  print_value("Show this help message\n");
    print_label("  clear    ");  print_value("Clear the screen\n");
    print_label("  echo     ");  print_value("Echo text (usage: echo <text>)\n");
    print_label("  info     ");  print_value("Display system information\n");
    print_label("  uptime   ");  print_value("Seconds since boot\n");
    print_label("  date     ");  print_value("Show date/time from CMOS RTC\n");
    print_label("  mem      ");  print_value("Show memory layout\n");
    print_label("  heap     ");  print_value("Bump allocator stats (extended memory)\n");
    print_label("  ls       ");  print_value("List ramdisk files\n");
    print_label("  cat      ");  print_value("Print a ramdisk file (usage: cat <name>)\n");
    print_label("  write    ");  print_value("Create/overwrite (usage: write <name> <text>)\n");
    print_label("            ");  print_value("  <name> is one word — no spaces in filenames.\n");
    print_label("  rm       ");  print_value("Remove a ramdisk file (usage: rm <name>)\n");
    print_label("  color    ");  print_value("Change text color (usage: color <0-15>)\n");
    print_label("  fortune  ");  print_value("Random computing quote\n");
    print_label("  hello    ");  print_value("A friendly greeting\n");
    print_label("  panic    ");  print_value("Trigger a (fake) kernel panic\n");
    print_label("  reboot   ");  print_value("Reboot the machine\n");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_echo(const char *args) {
    vga_putchar('\n');
    if (args) vga_print(args);
    vga_putchar('\n');
}

static void cmd_info(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("\n  System Information\n\n");

    print_label("  OS           ");  print_value("miniOS v0.2\n");
    print_label("  Architecture ");  print_value("i386 (32-bit protected mode)\n");
    print_label("  Display      ");  print_value("VGA text mode 80x25\n");
    print_label("  Keyboard     ");  print_value("PS/2 (IRQ 1, scancode set 1)\n");
    print_label("  Timer        ");  print_value("PIT channel 0 @ ~18.2 Hz\n");
    print_label("  Boot method  ");  print_value("Custom MBR bootloader\n");
    print_label("  Heap         ");  print_value("Bump allocator @ 1 MiB (512 KiB)\n");
    print_label("  Ramdisk      ");  print_value("In-memory flat FS (max 32 files)\n");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static void cmd_uptime(void) {
    uint32_t secs = timer_ticks / 18;
    uint32_t mins = secs / 60;
    secs %= 60;
    vga_print("\n  Uptime: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    print_num((int)mins);
    vga_print("m ");
    print_num((int)secs);
    vga_print("s");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print(" (");
    print_num((int)timer_ticks);
    vga_print(" ticks)\n");
}

static void cmd_date(void) {
    uint8_t yr  = bcd_to_bin(cmos_read(0x09));
    uint8_t mon = bcd_to_bin(cmos_read(0x08));
    uint8_t day = bcd_to_bin(cmos_read(0x07));
    uint8_t hr  = bcd_to_bin(cmos_read(0x04));
    uint8_t min = bcd_to_bin(cmos_read(0x02));
    uint8_t sec = bcd_to_bin(cmos_read(0x00));

    vga_print("\n  ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("20");
    print_num(yr);
    vga_putchar('-');
    if (mon < 10) vga_putchar('0');
    print_num(mon);
    vga_putchar('-');
    if (day < 10) vga_putchar('0');
    print_num(day);
    vga_print("  ");
    if (hr < 10) vga_putchar('0');
    print_num(hr);
    vga_putchar(':');
    if (min < 10) vga_putchar('0');
    print_num(min);
    vga_putchar(':');
    if (sec < 10) vga_putchar('0');
    print_num(sec);
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_putchar('\n');
}

static void cmd_mem(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("\n  Memory Layout\n\n");

    print_label("  0x00000000 - 0x000003FF  ");  print_value("Real-mode IVT\n");
    print_label("  0x00000400 - 0x000004FF  ");  print_value("BIOS data area\n");
    print_label("  0x00001000 - 0x00007BFF  ");  print_value("Kernel image\n");
    print_label("  0x00007C00 - 0x00007DFF  ");  print_value("Boot sector\n");
    print_label("  0x00010000 - 0x0008FFFF  ");  print_value("Stack (grows down from 0x90000)\n");
    print_label("  0x000B8000 - 0x000B8F9F  ");  print_value("VGA text buffer\n");
    print_label("  0x000C0000 - 0x000FFFFF  ");  print_value("BIOS ROM\n");
    print_label("  0x00100000 - 0x0017FFFF  ");  print_value("Kernel bump heap (512 KiB)\n");
    print_label("  0x00180000 - ??????????  ");  print_value("Extended memory (unused)\n");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print("  Ramdisk file contents live in the bump heap; ");
    vga_print("'rm' frees the name slot but does not reclaim bytes.\n\n");
}

static void cmd_heap(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("\n  Bump heap\n\n");

    print_label("  Base      ");
    print_value("0x00100000\n");
    print_label("  Used      ");
    print_uint32((uint32_t)kmalloc_used());
    vga_print(" bytes\n");
    print_label("  Capacity  ");
    print_uint32((uint32_t)kmalloc_capacity());
    vga_print(" bytes\n");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static int ls_count;

static void ls_cb(const char *name, size_t size, void *ctx) {
    (void)ctx;
    ls_count++;
    vga_print("    ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(name);
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print("  ");
    print_num((int)size);
    vga_print(" bytes\n");
}

static void cmd_ls(void) {
    vga_putchar('\n');
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("  Ramdisk files\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    ls_count = 0;
    ramdisk_foreach(ls_cb, NULL);
    if (ls_count == 0)
        vga_print("  (empty - use write <name> <text>)\n");
    vga_putchar('\n');
}

static void cmd_cat(const char *name) {
    const uint8_t *data;
    size_t len;

    if (!name || !*name) {
        vga_print("\n  Usage: cat <name>\n");
        return;
    }
    if (ramdisk_get(name, &data, &len) != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  cat: no such file: ");
        vga_print(name);
        vga_putchar('\n');
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
        return;
    }
    vga_putchar('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (size_t i = 0; i < len; i++)
        vga_putchar((char)data[i]);
    if (len == 0 || data[len - 1] != '\n')
        vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static void cmd_write(const char *args) {
    if (!args || !*args) {
        vga_print("\n  Usage: write <name> <text>\n");
        return;
    }
    const char *p = args;
    while (*p && *p != ' ')
        p++;
    if (*p == '\0') {
        vga_print("\n  Usage: write <name> <text>\n");
        return;
    }
    size_t name_len = (size_t)(p - args);
    if (name_len >= 32) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  write: name too long (max 31 chars)\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
        return;
    }
    char name[32];
    memcpy(name, args, name_len);
    name[name_len] = '\0';
    p = skip_ws(p);
    int rc = ramdisk_write(name, p, strlen(p));
    if (rc == -1) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  write: invalid name (no spaces or slashes)\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    } else if (rc == -2) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  write: ramdisk full (32 files)\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    } else if (rc == -3) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  write: out of heap\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    } else {
        vga_print("\n  Wrote ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(name);
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
        vga_print("\n");
    }
}

static void cmd_rm(const char *name) {
    if (!name || !*name) {
        vga_print("\n  Usage: rm <name>\n");
        return;
    }
    if (ramdisk_rm(name) != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  rm: no such file: ");
        vga_print(name);
        vga_putchar('\n');
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
        return;
    }
    vga_print("\n  Removed ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(name);
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_print("\n");
}

static void cmd_color(const char *args) {
    if (!args || !*args) {
        vga_print("\n  Usage: color <0-15>\n");
        vga_print("  0=Black  1=Blue  2=Green  3=Cyan  4=Red  5=Magenta\n");
        vga_print("  6=Brown  7=LtGray  8=DkGray  9=LtBlue  10=LtGreen\n");
        vga_print("  11=LtCyan  12=LtRed  13=LtMag  14=Yellow  15=White\n");
        return;
    }
    int c = 0;
    const char *p = args;
    while (*p >= '0' && *p <= '9')
        c = c * 10 + (*p++ - '0');
    if (c > 15) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  Invalid color (0-15).\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
        return;
    }
    vga_set_color((uint8_t)c, VGA_BLACK);
    vga_print("\n  Text color changed!\n");
}

static void cmd_fortune(void) {
    static const char *fortunes[] = {
        "The best way to predict the future is to invent it. - Alan Kay",
        "Any sufficiently advanced technology is indistinguishable from magic. - Arthur C. Clarke",
        "There are only 10 types of people: those who understand binary and those who don't.",
        "To iterate is human, to recurse divine. - L. Peter Deutsch",
        "UNIX is user-friendly. It's just very particular about who its friends are.",
        "There's no place like 127.0.0.1.",
        "In a world without fences and walls, who needs Gates and Windows?",
        "Hardware: the parts of a computer that can be kicked. - Jeff Pesis",
        "Computers are fast; programmers keep it slow.",
        "It works on my machine!",
    };
    int n = sizeof(fortunes) / sizeof(fortunes[0]);
    int i = simple_rand() % n;
    vga_putchar('\n');
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_print("  \"");
    vga_print(fortunes[i]);
    vga_print("\"\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static void cmd_hello(void) {
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_print("\n  Hello from miniOS! Have a wonderful day!\n");
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
}

static void cmd_panic(void) {
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_clear();
    vga_print("\n\n\n");
    vga_print("      *** KERNEL PANIC ***\n\n");
    vga_print("      An unrecoverable error has occurred.\n\n");
    vga_print("      Just kidding! This is a toy OS. :)\n\n");
    vga_print("      Press any key to continue...\n");

    while (!keyboard_getchar())
        __asm__ volatile("hlt");

    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_clear();
    print_banner();
}

static void cmd_reboot(void) {
    vga_print("\n  Rebooting...\n");
    uint8_t s;
    do { s = inb(0x64); } while (s & 0x02);
    outb(0x64, 0xFE);
}

/* ── Command dispatcher ────────────────────────────────────────────── */

static void execute(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if      (strcmp(cmd, "help")    == 0) cmd_help();
    else if (strcmp(cmd, "clear")   == 0) cmd_clear();
    else if (strcmp(cmd, "info")    == 0) cmd_info();
    else if (strcmp(cmd, "uptime")  == 0) cmd_uptime();
    else if (strcmp(cmd, "date")    == 0) cmd_date();
    else if (strcmp(cmd, "mem")     == 0) cmd_mem();
    else if (strcmp(cmd, "heap")    == 0) cmd_heap();
    else if (strcmp(cmd, "ls")      == 0) cmd_ls();
    else if (strncmp(cmd, "cat ", 4) == 0) cmd_cat(skip_ws(cmd + 4));
    else if (strcmp(cmd, "cat")     == 0) cmd_cat("");
    else if (strncmp(cmd, "write ", 6) == 0) cmd_write(skip_ws(cmd + 6));
    else if (strcmp(cmd, "write")   == 0) cmd_write("");
    else if (strncmp(cmd, "rm ", 3) == 0) cmd_rm(skip_ws(cmd + 3));
    else if (strcmp(cmd, "rm")      == 0) cmd_rm("");
    else if (strcmp(cmd, "fortune") == 0) cmd_fortune();
    else if (strcmp(cmd, "hello")   == 0) cmd_hello();
    else if (strcmp(cmd, "panic")   == 0) cmd_panic();
    else if (strcmp(cmd, "reboot")  == 0) cmd_reboot();
    else if (strncmp(cmd, "echo ", 5) == 0)  cmd_echo(cmd + 5);
    else if (strcmp(cmd, "echo")    == 0)     cmd_echo("");
    else if (strncmp(cmd, "color ", 6) == 0) cmd_color(cmd + 6);
    else if (strcmp(cmd, "color")   == 0)     cmd_color("");
    else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("\n  Unknown command: ");
        vga_print(cmd);
        vga_print("\n  Type 'help' for available commands.\n");
        vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    }
}

/* ── Entry point ───────────────────────────────────────────────────── */

void kernel_main(void) {
    vga_init();
    idt_init();
    kmalloc_init();
    ramdisk_init();

    print_banner();
    print_prompt();

    for (;;) {
        char c = keyboard_getchar();
        if (c == 0) {
            __asm__ volatile("hlt");
            continue;
        }

        if (c == '\n') {
            vga_putchar('\n');
            cmd_buf[cmd_len] = '\0';
            execute(cmd_buf);
            cmd_len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                vga_putchar('\b');
            }
        } else if (cmd_len < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_len++] = c;
            vga_putchar(c);
        }
    }
}
