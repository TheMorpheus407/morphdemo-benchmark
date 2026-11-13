/* Freestanding runtime shared by the C demos (64k / 16k).
 * No libc headers: every external function is imported by name at runtime
 * through dlopen/dlsym (see IMP() below and hdr.S).
 * With -DDEV the same code is built as a normal hosted program for the
 * development tools (frame dumps, wav dumps).                                 */
#ifndef SYS_H
#define SYS_H

typedef unsigned char u8;  typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long u64;
typedef signed char i8;    typedef short i16;          typedef int i32;          typedef long i64;
typedef float f32;         typedef double f64;
#ifndef NULL
#define NULL ((void*)0)
#endif

/* ---------------------------------------------------------------- imports */
/* Each IMP() reserves one pointer slot in .imp (bss) under the function's own
 * name and appends the name to .impnames; imp_load() walks both in order.    */
#define IMP_STR2(x) #x
#define IMP_STR(x) IMP_STR2(x)
#define LIB(soname) __asm__(".pushsection .impnames,\"a\",@progbits\n.byte 1\n.asciz \"" soname "\"\n.popsection\n");
#define IMPN(ret, cname, sym, args) extern ret (*cname) args; \
  __asm__(".pushsection .imp,\"aw\",@nobits\n.globl " #cname "\n" #cname ": .skip 8\n.popsection\n" \
          ".pushsection .impnames,\"a\",@progbits\n.asciz \"" sym "\"\n.popsection\n");
#define IMP(ret, name, args) IMPN(ret, name, #name, args)
#define IMP_BEGIN __asm__(".pushsection .imp,\"aw\",@nobits\n.globl imp_table\nimp_table:\n.popsection\n" \
                          ".pushsection .impnames,\"a\",@progbits\n.globl imp_names\nimp_names:\n.popsection\n");
#define IMP_END   __asm__(".pushsection .impnames,\"a\",@progbits\n.byte 0\n.popsection\n");

extern void *imp_table[];
extern const char imp_names[];

#ifdef DEV
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static void *(*imp_dlopen)(const char *, int) = dlopen;
static void *(*imp_dlsym)(void *, const char *) = dlsym;
#else
extern void *(*imp_dlopen)(const char *, int);
extern void *(*imp_dlsym)(void *, const char *);

static inline long sys1(long n, long a) { long r; __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a) : "rcx", "r11", "memory"); return r; }
static inline long sys3(long n, long a, long b, long c) { long r; __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory"); return r; }
__attribute__((noreturn)) static inline void sys_exit(int code) { for (;;) sys1(231, code); }

/* gcc may emit calls to these even in freestanding mode */
__attribute__((used, noinline)) void *memset(void *d, int c, unsigned long n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
__attribute__((used, noinline)) void *memcpy(void *d, const void *s, unsigned long n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; return d; }

int cmain(void);
__asm__(".pushsection .text.start,\"ax\",@progbits\n.globl _start\n_start:\n"
        "xor %ebp,%ebp\nand $-16,%rsp\ncall cmain\nmov %eax,%edi\nmov $231,%eax\nsyscall\n.popsection\n");
#endif

static void imp_load(void) {
  const char *s = imp_names;
  void **t = imp_table;
  void *h = 0;
  while (*s) {
    if (*s == 1) h = imp_dlopen(++s, 0x102 /* RTLD_NOW|RTLD_GLOBAL */);
    else *t++ = h ? imp_dlsym(h, s) : 0;
    while (*s++) {}
  }
}

#endif
