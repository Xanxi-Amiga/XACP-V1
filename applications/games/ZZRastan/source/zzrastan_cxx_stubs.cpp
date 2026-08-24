/*
 * zzrastan_cxx_stubs.cpp - minimal C++ runtime glue for ymfm on bare metal.
 * ymfm only pulls std::string/new/delete through its register-name debug helpers, which we never
 * call. Providing tiny stubs avoids dragging in the hosted C++ runtime.
 * ASCII only.
 */
#include <stddef.h>

extern "C" void *malloc(size_t);
extern "C" void  free(void *);

void *operator new(size_t n)   { return malloc(n); }
void *operator new[](size_t n) { return malloc(n); }
void  operator delete(void *p)            { free(p); }
void  operator delete[](void *p)          { free(p); }
void  operator delete(void *p, size_t)    { free(p); }
void  operator delete[](void *p, size_t)  { free(p); }

extern "C" void __cxa_pure_virtual(void) { for (;;) { } }

namespace std {
    void __throw_length_error(const char *) { for (;;) { } }
    void __throw_logic_error(const char *)  { for (;;) { } }
    void __throw_bad_alloc()                { for (;;) { } }
}

/* Bare-metal glue: no shared objects, no static destructors, no _fini. */
extern "C" {
    void *__dso_handle = 0;
    void _fini(void) { }
    void _init(void) { }
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
}

/* ymfm's register-name debug helper is the only thing pulling std::string; it is never called
   from our code path, so a stub satisfies the linker without the hosted C++ runtime. */
namespace std { namespace __cxx11 {
    class __zzr_dummy {};
} }
extern "C" char *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE9_M_createERjj(void)
{
    for (;;) { }
}
