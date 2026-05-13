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
          -Wall -Wextra -O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -msoft-float -Ikernel
LDFLAGS = -m elf_i386

BUILD   = build

# ── Sources & objects ────────────────────────────────────────────────

BOOT_SRC    = boot/boot.asm
ENTRY_SRC   = kernel/entry.asm
KERNEL_SRCS = kernel/kernel.c kernel/vga.c kernel/keyboard.c \
              kernel/idt.c kernel/string.c kernel/kmalloc.c kernel/ramdisk.c \
              kernel/minivm.c kernel/ide.c kernel/fat16.c

ENTRY_OBJ   = $(BUILD)/entry.o
KERNEL_OBJS = $(patsubst kernel/%.c,$(BUILD)/%.o,$(KERNEL_SRCS))

BOOT_BIN    = $(BUILD)/boot.bin
KERNEL_ELF  = $(BUILD)/kernel.elf
KERNEL_BIN  = $(BUILD)/kernel.bin
KERNEL_PAD  = $(BUILD)/kernel.padded.bin
OS_IMAGE    = $(BUILD)/os.img
HDD_IMG     = $(BUILD)/hdd.img
# Standard 1.44 MiB floppy size (80 cyl * 2 heads * 18 sectors * 512 bytes).
FLOPPY_SIZE = 1474560

# ── Targets ──────────────────────────────────────────────────────────

.PHONY: all run debug clean docker hdd-img hdd-docker

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

# Pad kernel binary to a sector boundary (whole 512-byte blocks on the floppy).
$(KERNEL_PAD): $(KERNEL_BIN)
	dd if=$< of=$@ bs=512 conv=sync

# Pad to a standard 1.44 MiB geometry so SeaBIOS/QEMU accept the floppy (avoids
# hangs at "Booting from Floppy..."). Also satisfies the BIOS read span from sector 2.
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_PAD)
	cat $^ > $@
	@SZ=$$(wc -c < $@ | awk '{print $$1}'); \
	if [ $$SZ -lt $(FLOPPY_SIZE) ]; then \
	  dd if=/dev/zero bs=$$(($(FLOPPY_SIZE) - $$SZ)) count=1 >> $@; \
	fi

# Partitionless FAT16 image (mkfs.vfat -C). Last arg is 1024-byte blocks (see mkfs.vfat --help).
# 8192 blocks (8 MiB) is rejected by dosfstools 4.x as too small for FAT16; 16384 = 16 MiB.
$(HDD_IMG): | $(BUILD)
	@if command -v mkfs.vfat >/dev/null 2>&1; then \
	  rm -f $@; \
	  mkfs.vfat -F 16 -C $@ 16384; \
	else \
	  echo "Missing mkfs.vfat. Install dosfstools, or run: make hdd-docker"; \
	  exit 1; \
	fi

hdd-img: $(HDD_IMG)

hdd-docker: | $(BUILD)
	docker run --rm -v "$$(pwd)":/os -w /os ubuntu:22.04 bash -lc \
	  'apt-get update -qq && apt-get install -y -qq dosfstools && rm -f $(HDD_IMG) && mkfs.vfat -F 16 -C $(HDD_IMG) 16384'

# -no-reboot: triple-fault shows an error instead of a BIOS/shell flash loop.
# menu=off: avoids SeaBIOS boot menu popping over the guest (looks like flicker).
QEMU_BASE = qemu-system-i386 -no-reboot \
	  -drive file=$(OS_IMAGE),format=raw,if=floppy \
	  -boot order=a,menu=off \
	  -drive file=$(HDD_IMG),format=raw,if=ide,index=0,media=disk,cache=unsafe

run: $(OS_IMAGE) $(HDD_IMG)
	$(QEMU_BASE)

debug: $(OS_IMAGE) $(HDD_IMG)
	$(QEMU_BASE) \
	  -s -S -monitor stdio

docker:
	docker build -t minios-builder .
	docker run --rm -v "$$(pwd)":/os minios-builder

clean:
	rm -rf $(BUILD)
