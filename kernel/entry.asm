bits 32
section .text

extern kernel_main
extern keyboard_irq_handler
extern timer_tick_handler
extern __bss_start
extern __bss_end

global _start
global isr_timer_stub
global isr_keyboard_stub

_start:
    cld
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor al, al
    rep stosb

    call kernel_main
    jmp $

isr_timer_stub:
    pusha
    cld
    call timer_tick_handler
    popa
    iret

isr_keyboard_stub:
    pusha
    cld
    call keyboard_irq_handler
    popa
    iret
