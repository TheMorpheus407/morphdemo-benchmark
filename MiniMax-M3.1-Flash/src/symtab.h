/* symtab.h -- runtime symbol table.
 *
 * The environment has no development headers for X11, GL or ALSA, and the
 * size constrained binaries cannot afford a PLT entry per imported symbol.
 * Both problems are solved the same way: the handful of entry points that
 * are needed are named once in a X-macro list and resolved at start up with
 * dlsym() against the already loaded libraries.  The names live in one blob
 * in .rodata, the resolved pointers in .bss.
 *
 * A translation unit defines SYMS(X) before including this file, where each
 * entry is X(name, return type, (argument types)).  Exactly one translation
 * unit defines MORPH_SYMTAB_DEFINE and owns the table; everybody else just
 * reads it.
 */
#ifndef MORPH_SYMTAB_H
#define MORPH_SYMTAB_H

#include <dlfcn.h>

#ifndef SYMS
#error "SYMS(X) must be defined before including symtab.h"
#endif

enum {
#define X(n, r, p) I_##n,
    SYMS(X)
#undef X
    NSYMS
};

#define X(n, r, p) typedef r (*PFN_##n) p;
SYMS(X)
#undef X

#ifdef MORPH_SYMTAB_DEFINE

void *g_sym[NSYMS];

/* The names are emitted as adjacent string literals, which the compiler
 * concatenates into a single blob in .rodata.  One loop then resolves the
 * whole table, so a symbol costs its name and nothing else. */
static const char g_names[] =
#define X(nm, r, p) #nm "\0"
    SYMS(X)
#undef X
    ;

void sym_load(void) {
    const char *p = g_names;
    int i;
    for (i = 0; i < NSYMS; i++) {
        g_sym[i] = dlsym(RTLD_DEFAULT, p);
        while (*p++) ;
    }
}

#else

extern void *g_sym[NSYMS];

#endif /* MORPH_SYMTAB_DEFINE */

#define SYM(n, ...) ((PFN_##n)g_sym[I_##n])(__VA_ARGS__)

#endif /* MORPH_SYMTAB_H */
