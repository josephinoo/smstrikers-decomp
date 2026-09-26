// Metrowerks C/C++ runtime pieces the decomp calls directly. GCC's runtime has
// no equivalent, so they are reimplemented here.

#include "types.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

extern "C" {

// Leave enough system memory for vitaGL while accommodating the game's arena.
__attribute__((used)) unsigned int _newlib_heap_size_user = 104 * 1024 * 1024;

// The original game has deep loading call chains that overflow VitaSDK's
// default main-thread stack while loading frontend camera animations.
__attribute__((used)) unsigned int sceUserMainThreadStackSize = 4 * 1024 * 1024;

// Lowest normal positive float, the companion of __float_max in link_stubs.cpp.
int32_t __float_min[1] = { 0x00800000 };

// PowerPC FPSCR exception-enable mask. Nothing on ARM reads it; the game only
// ever ORs bits into it, so a plain variable is the faithful behaviour.
u32 __OSFpscrEnableBits = 0;

// MW's double -> unsigned conversion helper. C's own conversion is undefined
// for out-of-range values, so clamp the way the PPC instruction does.
u32 __cvt_fp2unsigned(f64 d)
{
    if (d <= 0.0) {
        return 0u;
    }
    if (d >= 4294967295.0) {
        return 0xFFFFFFFFu;
    }
    return static_cast<u32>(d);
}

// MW emits this for `new T[n]` when T has a non-trivial constructor: the block
// starts with an n-sized cookie, then n objects are constructed in place.
typedef void* ConstructorDestructor;

void* __construct_new_array(void* block, ConstructorDestructor ctor,
                            ConstructorDestructor dtor, size_t size, size_t n)
{
    (void)dtor;
    if (block == nullptr) {
        return nullptr;
    }

    auto* cookie = static_cast<size_t*>(block);
    *cookie = n;
    auto* objects = reinterpret_cast<char*>(cookie + 1);

    if (ctor != nullptr) {
        auto construct = reinterpret_cast<void (*)(void*)>(ctor);
        for (size_t i = 0; i < n; ++i) {
            construct(objects + i * size);
        }
    }
    return objects;
}

// GCC leaves __cxa_pure_virtual weak and undefined, so a pure-virtual call
// silently branches to address 0 and Vita3K reports only "PC is 0x0". Define it
// so the failure names itself instead.
void __cxa_pure_virtual(void)
{
    std::printf("[FATAL] pure virtual function called\n");
    std::abort();
}

} // extern "C"
