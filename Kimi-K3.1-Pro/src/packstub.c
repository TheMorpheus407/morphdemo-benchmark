#include <stdint.h>
#include "packed.h"

#define LITERAL_BASE 256
#define LENGTH_BASE 65792
#define DISTANCE_BASE 69920
#define MODEL_SIZE 74048

static uint16_t probabilities[MODEL_SIZE];
static uint8_t output[PAYLOAD_SIZE];
static const uint8_t *input;
static uint32_t range, code;

static uint8_t next_byte(void) {
    return *input++;
}

static unsigned decode_bit(unsigned context) {
    uint32_t p = probabilities[context];
    uint32_t bound = (range >> 12) * p;
    unsigned bit;
    if (code < bound) {
        bit = 0;
        range = bound;
        probabilities[context] = p + ((4096 - p) >> 3);
    } else {
        bit = 1;
        code -= bound;
        range -= bound;
        probabilities[context] = p - (p >> 3);
    }
    while (range < 0x1000000) {
        range <<= 8;
        code = (code << 8) | next_byte();
    }
    return bit;
}

static unsigned gamma(unsigned base) {
    unsigned n = 0, prefix = 1;
    while (!decode_bit(base + n)) ++n;
    for (unsigned i = 0; i < n; ++i) {
        unsigned bit = decode_bit(base + 32 + (n << 8) + (prefix & 255));
        prefix = (prefix << 1) | bit;
    }
    return prefix;
}

static void decompress(void) {
    for (unsigned i = 0; i < MODEL_SIZE; ++i) probabilities[i] = 2048;
    input = compressed;
    range = 0xffffffffu;
    code = 0;
    for (unsigned i = 0; i < 5; ++i) code = (code << 8) | next_byte();
    uint8_t *out = output;
    unsigned previous = 0;
    while (out < output + PAYLOAD_SIZE) {
        if (decode_bit(previous)) {
            unsigned length = gamma(LENGTH_BASE) + 1;
            unsigned offset = gamma(DISTANCE_BASE);
            while (length--) {
                previous = out[-(long)offset];
                *out++ = previous;
            }
        } else {
            unsigned prefix = 1;
            for (unsigned i = 0; i < 8; ++i) {
                unsigned bit = decode_bit(LITERAL_BASE + (previous << 8) + prefix);
                prefix = (prefix << 1) | bit;
            }
            previous = prefix & 255;
            *out++ = previous;
        }
    }
}

static long syscall5(long number, long a1, long a2, long a3, long a4, long a5) {
    long result;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    __asm__ volatile("syscall" : "=a"(result)
                     : "a"(number), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
                     : "rcx", "r11", "memory");
    return result;
}

__attribute__((noreturn, used)) void stub_start(uint64_t *stack) {
    decompress();
    long fd = syscall5(319, (long)"m", 0, 0, 0, 0);
    char **argv = (char **)&stack[1];
    char **envp = argv + stack[0] + 1;
    if (fd >= 0 && syscall5(1, fd, (long)output, PAYLOAD_SIZE, 0, 0) == PAYLOAD_SIZE)
        syscall5(322, fd, (long)"", (long)argv, (long)envp, 0x1000);
    syscall5(60, 1, 0, 0, 0, 0);
    __builtin_unreachable();
}

__asm__(
    ".global _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "mov %rsp,%rdi\n"
    "and $-16,%rsp\n"
    "call stub_start\n"
    ".section .note.GNU-stack,\"\",@progbits\n"
);
