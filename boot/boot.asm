[org 0x7c00]
bits 16

; SeaBIOS / PC BIOS expect byte 0 to be a short jump (0xEB) on floppy boot sectors.
jmp short boot_start
nop

KERNEL_OFFSET equ 0x1000
KERNEL_SECTORS equ 48

boot_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov bp, sp

    mov [BOOT_DRIVE], dl

    mov si, MSG_BOOT
    call print_rm

    call load_kernel

    call switch_to_pm

    jmp $

; ── Real-mode helpers ────────────────────────────────────────────────

print_rm:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_kernel:
    mov si, MSG_LOAD
    call print_rm

    xor ax, ax
    mov es, ax
    mov bx, KERNEL_OFFSET
    mov si, KERNEL_SECTORS  ; sectors to read (24 KB kernel budget)
    mov ch, 0               ; cylinder
    mov dh, 0               ; head
    mov cl, 2               ; sector (1-based; boot sector is sector 1)

.read_next:
    mov ah, 0x02            ; BIOS read sectors
    mov al, 1               ; read exactly one sector; do not cross tracks
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .disk_err

    add bx, 512
    inc cl
    cmp cl, 19              ; 18 sectors per track, next sector would be 19
    jne .same_track
    mov cl, 1
    xor dh, 1               ; switch head 0 <-> 1
    jnz .same_track
    inc ch                  ; after head 1 wraps to 0, advance cylinder

.same_track:
    dec si
    jnz .read_next
    ret

.disk_err:
    mov si, MSG_DISK_ERR
    call print_rm
    jmp $

; ── GDT ──────────────────────────────────────────────────────────────

gdt_start:

gdt_null:
    dq 0

gdt_code:                       ; code segment: base 0, limit 4 GB
    dw 0xffff                   ; limit  0:15
    dw 0x0000                   ; base   0:15
    db 0x00                     ; base  16:23
    db 10011010b                ; access: present, ring 0, code, readable
    db 11001111b                ; flags + limit 16:19  (4 KB gran, 32-bit)
    db 0x00                     ; base  24:31

gdt_data:                       ; data segment: base 0, limit 4 GB
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10010010b                ; access: present, ring 0, data, writable
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ── Protected-mode switch ────────────────────────────────────────────

switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

bits 32

init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp

    call KERNEL_OFFSET
    jmp $

; ── Data ─────────────────────────────────────────────────────────────

BOOT_DRIVE:    db 0
MSG_BOOT:      db "Booting miniOS...", 13, 10, 0
MSG_LOAD:      db "Loading kernel...", 13, 10, 0
MSG_DISK_ERR:  db "Disk read error!", 0

; ── Boot signature ───────────────────────────────────────────────────

times 510 - ($ - $$) db 0
dw 0xaa55
