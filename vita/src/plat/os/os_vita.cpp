#include <dolphin/os.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <cstdlib>
#include <cstring>

volatile OSHeapHandle __OSCurrHeap = 0;

namespace {

// The GameCube handed the game a contiguous arena between OSGetArenaLo() and
// OSGetArenaHi(), and nlInitMemory() carves its entire heap out of it. Reserve
// a real block up front so those bounds mean something on the Vita.
constexpr unsigned long kArenaSize = 32u * 1024u * 1024u;

char* g_arena_lo = nullptr;
char* g_arena_hi = nullptr;

void ensure_arena()
{
    if (g_arena_lo != nullptr) {
        return;
    }
    g_arena_lo = static_cast<char*>(std::malloc(kArenaSize));
    g_arena_hi = g_arena_lo != nullptr ? g_arena_lo + kArenaSize : nullptr;
}

} // namespace

extern "C" {

void* OSGetArenaLo(void)
{
    ensure_arena();
    return g_arena_lo;
}

void* OSGetArenaHi(void)
{
    ensure_arena();
    return g_arena_hi;
}

void OSSetArenaLo(void* lo)
{
    g_arena_lo = static_cast<char*>(lo);
}

void* OSAllocFromHeap(int /*heap*/, u32 size)
{
    return std::malloc(size ? size : 1);
}

void OSFreeToHeap(int /*heap*/, void* ptr)
{
    std::free(ptr);
}

void* OSAllocFixed(void* /*rstart*/, void* /*rend*/)
{
    return nullptr;
}

int OSSetCurrentHeap(int heap)
{
    __OSCurrHeap = heap;
    return heap;
}

void* OSInitAlloc(void* arenaStart, void* /*arenaEnd*/, int /*maxHeaps*/)
{
    return arenaStart;
}

int OSCreateHeap(void* /*start*/, void* /*end*/)
{
    return 0;
}

void OSDestroyHeap(int /*heap*/) {}
void OSAddToHeap(int /*heap*/, void* /*start*/, void* /*end*/) {}

s32 OSCheckHeap(int /*heap*/)
{
    return 0;
}

u32 OSReferentSize(void* /*ptr*/)
{
    return 0;
}

void OSDumpHeap(int /*heap*/) {}
void OSVisitAllocated(void (* /*visitor*/)(void*, u32)) {}

OSTime OSGetTime(void)
{
    return static_cast<OSTime>(sceKernelGetProcessTimeWide());
}

void OSYieldThread(void)
{
    sceKernelDelayThread(0);
}

u32 OSGetTick(void)
{
    return static_cast<u32>(sceKernelGetProcessTimeLow());
}

int sceAppMgrAppParamGetString(int /*pid*/, int param, char *string, int len)
{
    if (string && len > 0) {
        if (param == 12) { // 12 = TITLE_ID
            std::strncpy(string, "VSTR00003", len - 1);
            string[len - 1] = '\0';
            return 0;
        }
        string[0] = '\0';
    }
    return 0;
}

} // extern "C"
