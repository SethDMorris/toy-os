# miniOS

A toy operating system built from scratch in x86 assembly and C.

## Features

- **Custom bootloader** — loads the kernel from disk and switches to 32-bit protected mode
- **VGA text-mode driver** — 80x25 colored output with scrolling
- **PS/2 keyboard driver** — interrupt-driven input with shift support
- **Interactive shell** — built-in commands including `help`, `info`, `date`, `fortune`, and more
- **Hardware interrupts** — PIC remapping, IDT, timer tick counting
- **CMOS RTC** — reads the real-time clock for the `date` command

## Quick Start

### Prerequisites

| Tool | Install (macOS) | Install (Ubuntu/Debian) |
|------|-----------------|------------------------|
| NASM | `brew install nasm` | `apt install nasm` |
| Cross-compiler | `brew install x86_64-elf-gcc x86_64-elf-binutils` | `apt install gcc gcc-multilib` |
| QEMU | `brew install qemu` | `apt install qemu-system-x86` |

### Build & Run

```bash
make        # build the OS image
make run    # boot it in QEMU
```

### Docker Build (no cross-compiler needed)

```bash
make docker         # builds inside a container
qemu-system-i386 -fda build/os.img   # run locally
```

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear the screen |
| `echo <text>` | Echo text back |
| `info` | System information |
| `uptime` | Time since boot |
| `date` | Date/time from CMOS RTC |
| `mem` | Memory layout map |
| `color <0-15>` | Change text color |
| `fortune` | Random computing quote |
| `hello` | A friendly greeting |
| `panic` | Fake kernel panic (press any key to recover) |
| `reboot` | Reboot the machine |

## Project Structure

```
toy-os/
├── boot/
│   └── boot.asm          # MBR bootloader (real → protected mode)
├── kernel/
│   ├── entry.asm          # Kernel entry point + ISR stubs
│   ├── kernel.c           # Main kernel & shell
│   ├── vga.c / vga.h      # VGA text-mode driver
│   ├── keyboard.c / .h    # PS/2 keyboard driver
│   ├── idt.c / idt.h      # Interrupt descriptor table + PIC
│   ├── string.c / .h      # String utilities
│   └── ports.h            # I/O port inline functions
├── linker.ld              # Kernel linker script
├── Makefile
├── Dockerfile
└── README.md
```

## How It Works

1. **BIOS** loads the 512-byte boot sector to `0x7C00`
2. **Bootloader** reads the kernel from disk to `0x1000`, sets up a GDT, enables protected mode, and jumps to the kernel
3. **Kernel entry** zeroes BSS, then calls `kernel_main()`
4. **Kernel** initializes VGA, remaps the PIC, loads the IDT, enables interrupts, and enters the shell loop
5. **Shell** reads keystrokes via an interrupt-driven ring buffer and dispatches commands
