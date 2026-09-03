
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include "MyStdlib.h"

void exit(int status) {
}

void *my_malloc (unsigned long size) {
	return AllocVec(size, MEMF_PUBLIC|MEMF_CLEAR);
}

void my_free (void* ptr) {
	FreeVec(ptr);
}

void *my_memset (void *dest, int val, unsigned long len) {
	unsigned char *ptr = dest;
	while (len-- > 0) *ptr++ = val;
	return dest;
}

void *my_memcpy (void *dest, const void *src, unsigned long len) {
	char *d = dest;
	const char *s = src;
	while (len--) *d++ = *s++;
	return dest;
}

#if 0
int my_memcmp(const void *s1, const void *s2, unsigned long n) {
	unsigned char u1, u2;

	for ( ; n-- ; s1++, s2++) {
		u1 = * (unsigned char *) s1;
		u2 = * (unsigned char *) s2;
		if (u1 != u2) {
			return (u1-u2);
		}
	}
	return 0;
}
#endif

char *my_strcpy (char * dest, const char * src) {
	return my_memcpy (dest, src, my_strlen (src) + 1);
}

char *my_strncpy(char *dst, const char *src, unsigned long n) {
	if (n != 0) {
		char *d = dst;
		const char *s = src;

		do {
			if ((*d++ = *s++) == '\0') {
				/* NUL pad the remaining n-1 bytes */
				while (--n != 0)
					*d++ = '\0';
				break;
			}
		} while (--n != 0);
	}
	return dst;
}

unsigned long my_strlen(const char *str) {
	const char *s;
	for (s = str; *s; ++s);
	return (s - str);
}

int my_strncmp(const char *s1, const char *s2, unsigned long n) {
	if (n == 0) return (0);
	do {
		if (*s1 != *s2++) return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
		if (*s1++ == '\0') break;
	} while (--n != 0);
	return 0;
}

int my_abs (int i) {
	return i < 0 ? -i : i;
}

static unsigned long int next = 1;
int	my_rand(void) {
	next = next * 1103515245 + 12345;
	return (unsigned int)(next/65536) % 32768;
}

void my_srand(unsigned int seed) {
	next = seed;
}

