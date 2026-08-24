#include <stddef.h>
#include "zzrastan_shared.h"

typedef unsigned char u8;

void *memset(void *d, int c, size_t n)
{
    u8 *p = (u8 *)d;
    while (n-- != 0u)
        *p++ = (u8)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    u8 *pd = (u8 *)d;
    const u8 *ps = (const u8 *)s;
    while (n-- != 0u)
        *pd++ = *ps++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    u8 *pd = (u8 *)d;
    const u8 *ps = (const u8 *)s;
    if (pd == ps || n == 0u)
        return d;
    if (pd < ps) {
        while (n-- != 0u)
            *pd++ = *ps++;
    } else {
        pd += n;
        ps += n;
        while (n-- != 0u)
            *--pd = *--ps;
    }
    return d;
}

static char *heap_ptr;
void *_sbrk(int incr)
{
    char *old;
    if (heap_ptr == 0)
        heap_ptr = (char *)ZZR_ARM_HEAP;
    old = heap_ptr;
    if ((zzr_u32)(heap_ptr + incr) > ZZR_ARM_HEAP + ZZR_ARM_HEAP_SIZE)
        return (void *)-1;
    heap_ptr += incr;
    return old;
}

int _close(int f) { (void)f; return -1; }
int _read(int f, char *b, int l) { (void)f; (void)b; (void)l; return 0; }
int _write(int f, char *b, int l) { (void)f; (void)b; return l; }
int _lseek(int f, int o, int w) { (void)f; (void)o; (void)w; return 0; }
int _fstat(int f, void *s) { (void)f; (void)s; return 0; }
int _isatty(int f) { (void)f; return 1; }
void _exit(int c) { (void)c; for (;;) __asm__ volatile("wfe"); }
int _kill(int p, int s) { (void)p; (void)s; return -1; }
int _getpid(void) { return 1; }
