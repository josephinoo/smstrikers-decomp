// Catch-all link stubs for sms_entry path (filled as linker reports undefineds).
// ponytail: expand only from real undefined-ref lists; delete when real TUs land.

#include "plat_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// MSL float tables (referenced by GameCube math.h / AiUtil)
extern "C" {
int32_t __float_max[1] = { 0x7F7FFFFF };
int32_t __float_huge[1] = { 0x7F800000 };
int32_t __float_nan[1] = { 0x7FC00000 };
int32_t __double_huge[2] = { 0, 0x7FF00000 };
int32_t __extended_huge[4] = { 0, 0, 0, 0x7FFF0000 };

/* GC cache / PPC — no-ops on Vita (declared in dolphin/os/OSCache.h, PPCArch.h). */
void DCInvalidateRange(void*, unsigned int) { }
void DCFlushRange(void*, unsigned int) { }
void DCStoreRange(void*, unsigned int) { }
void DCFlushRangeNoSync(void*, unsigned int) { }
void DCStoreRangeNoSync(void*, unsigned int) { }
void DCZeroRange(void*, unsigned int) { }
void DCTouchRange(void*, unsigned int) { }
void ICInvalidateRange(void*, unsigned int) { }
void PPCSync() { }
void PPCHalt() { }
}

#include <psp2/kernel/clib.h>
#include <cstdarg>
#include <cstdio>

int nlPrintf(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int ret = std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sceClibPrintf("%s", buf);
    return ret;
}

#include "NL/nlFileGC.h"
#include <dolphin/pad.h>

void nlRegHandleDVDMessageCB(const Function<void(int)>& cb) {}
void nlRegHandleDVDAllClearCB(const Function<void(int)>& cb) {}
void nlRegCheckForResetFromFSCB(const Function<FnVoidVoid>& cb) {}

extern void InitPlatPad(void);
extern "C" BOOL PADInit(void) {
    InitPlatPad();
    return 1;
}

extern "C" PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    return callback;
}

extern "C" void vita_link_stubs_anchor(void)
{
    std::printf("[STUB] link_stubs loaded\n");
}
