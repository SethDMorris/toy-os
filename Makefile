# ── miniOS build system ──────────────────────────────────────────────
#
# Toolchain detection:  i686-elf-*  >  x86_64-elf-*  >  native gcc
#

ifneq ($(shell command -v i686-elf-gcc 2>/dev/null),)
    CROSS     = i686-elf-
    ARCH_FLAG =
else ifneq ($(shell command -v x86_64-elf-gcc 2>/dev/null),)
    CROSS     = x86_64-elf-
    ARCH_FLAG = -m32
else
    CROSS     =
    ARCH_FLAG = -m32
endif

CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
ASM     = nasm

CFLAGS  = $(ARCH_FLAG) -ffreestanding -fno-pie -fno-stack-protector \
          -Wall -Wextra -O2 -Ikernel
LDFLAGS = -m elf_i386

BUILD   = build

# ── Sources & objects ────────────────────────────────────────────────

BOOT_SRC    = boot/boot.asm
ENTRY_SRC   = kernel/entry.asm
KERNEL_SRCS = kernel/kernel.c kernel/vga.c kernel/keyboard.c \
              kernel/idt.c kernel/string.c kernel/kmalloc.c kernel/ramdisk.c

ENTRY_OBJ   = $(BUILD)/entry.o
KERNEL_OBJS = $(patsubst kernel/%.c,$(BUILD)/%.o,$(KERNEL_SRCS))

BOOT_BIN    = $(BUILD)/boot.bin
KERNEL_ELF  = $(BUILD)/kernel.elf
KERNEL_BIN  = $(BUILD)/kernel.bin
OS_IMAGE    = $(BUILD)/os.img

# ── Targets ──────────────────────────────────────────────────────────

.PHONY: all run debug clean docker

all: $(OS_IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BOOT_BIN): $(BOOT_SRC) | $(BUILD)
	$(ASM) -f bin $< -o $@

$(ENTRY_OBJ): $(ENTRY_SRC) | $(BUILD)
	$(ASM) -f elf32 $< -o $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $^

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $^ > $@

run: $(OS_IMAGE)
	qemu-system-i386 -drive file=$(OS_IMAGE),format=raw,if=floppy -boot order=a

debug: $(OS_IMAGE)
	qemu-system-i386 -drive file=$(OS_IMAGE),format=raw,if=floppy -boot order=a -s -S -monitor stdio

docker:
	docker build -t minios-builder .
	docker run --rm -v "$$(pwd)":/os minios-builder

clean:
	rm -rf $(BUILD)
