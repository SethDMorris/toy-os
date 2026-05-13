#ifndef MINIVM_H
#define MINIVM_H

#include <stddef.h>
#include <stdint.h>

/*
 * Stack bytecode for miniOS (phase 1).
 *
 * Opcodes (bytes in ramdisk .bc files):
 *   0x00 HALT     — stop VM
 *   0x01 PUSH32  — next 4 bytes: uint32 little-endian value, pushed as int32
 *   0x02 ADD      — pop b, pop a, push a+b
 *   0x03 SUB      — pop b, pop a, push a-b
 *   0x04 MUL      — pop b, pop a, push a*b (32-bit wrap)
 *   0x05 PRINT    — pop one value, print as signed decimal + newline
 *
 * Text source (for compile): one statement per line, # comments, blank lines ok.
 *   push 10
 *   push 3
 *   add
 *   print
 *   halt
 */

int minivm_run(const uint8_t *code, size_t len);

/* Compile human-readable BC text into bytes. Returns 0 on success. */
int minivm_compile(const char *src, size_t srclen,
                   uint8_t *out, size_t outcap, size_t *outlen);

#endif
