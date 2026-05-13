#include "idt.h"
#include "ports.h"
#include <stdint.h>

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

extern void isr_timer_stub(void);
extern void isr_keyboard_stub(void);

static void idt_set(uint8_t n, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[n].base_lo = handler & 0xFFFF;
    idt[n].base_hi = (handler >> 16) & 0xFFFF;
    idt[n].sel     = sel;
    idt[n].zero    = 0;
    idt[n].flags   = flags;
}

static void pic_remap(void) {
    outb(0x20, 0x11);  io_wait();
    outb(0xA0, 0x11);  io_wait();
    outb(0x21, 0x20);  io_wait();   /* master offset 32 */
    outb(0xA1, 0x28);  io_wait();   /* slave  offset 40 */
    outb(0x21, 0x04);  io_wait();
    outb(0xA1, 0x02);  io_wait();
    outb(0x21, 0x01);  io_wait();
    outb(0xA1, 0x01);  io_wait();
    outb(0x21, 0xFC);               /* enable IRQ 0 (timer) + IRQ 1 (kbd) */
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set(i, 0, 0, 0);

    pic_remap();

    /* 0x08 = kernel code selector, 0x8E = present + ring-0 + 32-bit int gate */
    idt_set(32, (uint32_t)isr_timer_stub,    0x08, 0x8E);
    idt_set(33, (uint32_t)isr_keyboard_stub, 0x08, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idtp));
    __asm__ volatile("sti");
}
