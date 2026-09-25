// Stubs for Dolphin SDK link surface.
// These are not implementations and simply return neutral values.

#include <dolphin.h>
#include <dolphin/thp.h>

extern "C" {

// OSGetArenaLo/Hi/OSSetArenaLo live in plat/os/os_vita.cpp: the game builds
// its whole heap out of them, so they need a real arena, not a stub.


u32 AIGetDMAStartAddr(void) { return 0; }
u32 AIGetDSPSampleRate(void) { return 0; }
void AIInitDMA(u32 start_addr, u32 length) {}
AIDCallback AIRegisterDMACallback(AIDCallback callback) { return nullptr; }
void AIStartDMA(void) {}

void ARQSetChunkSize(u32 size) {}

// CARD*: ACGC pattern (pc_card.c). MemCard::BeginCardAccess hangs forever if
// *Async APIs return READY but never fire their callbacks — SMS was stuck on
// "Checking the Memory Card" for exactly that reason.
static void card_fire(CARDCallback cb, s32 chan, s32 result)
{
    if (cb) {
        cb(chan, result);
    }
}

s32 CARDCheckAsync(s32 chan, CARDCallback callback)
{
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDClose(CARDFileInfo* fileInfo)
{
    (void)fileInfo;
    return CARD_RESULT_READY;
}
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback)
{
    (void)fileName;
    (void)size;
    (void)fileInfo;
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback)
{
    (void)fileName;
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDFormatAsync(s32 chan, CARDCallback callback)
{
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed)
{
    (void)chan;
    if (byteNotUsed) {
        *byteNotUsed = 1024 * 1024;
    }
    if (filesNotUsed) {
        *filesNotUsed = 100;
    }
    return CARD_RESULT_READY;
}
s32 CARDGetSerialNo(s32 chan, u64* serialNo)
{
    (void)chan;
    if (serialNo) {
        *serialNo = 0x534D5356ull; // 'SMSV'
    }
    return CARD_RESULT_READY;
}
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat)
{
    (void)chan;
    (void)fileNo;
    if (stat) {
        *stat = {};
    }
    return CARD_RESULT_READY;
}
s32 CARDGetXferredBytes(s32 chan)
{
    (void)chan;
    return 0;
}
void CARDInit(void) {}
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback)
{
    (void)workArea;
    (void)detachCallback;
    card_fire(attachCallback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo)
{
    (void)chan;
    (void)fileName;
    (void)fileInfo;
    // No save file yet — same as empty card slot in ACGC.
    return CARD_RESULT_NOFILE;
}
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize)
{
    (void)chan;
    if (memSize) {
        *memSize = 16 * 1024 * 1024;
    }
    if (sectorSize) {
        *sectorSize = 8192;
    }
    return CARD_RESULT_READY;
}
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback)
{
    (void)fileInfo;
    (void)buf;
    (void)length;
    (void)offset;
    s32 chan = fileInfo ? fileInfo->chan : 0;
    card_fire(callback, chan, CARD_RESULT_NOFILE);
    return CARD_RESULT_NOFILE;
}
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback)
{
    (void)fileNo;
    (void)stat;
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}
s32 CARDUnmount(s32 chan)
{
    (void)chan;
    return CARD_RESULT_READY;
}
s32 CARDWriteAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback)
{
    (void)buf;
    (void)length;
    (void)offset;
    s32 chan = fileInfo ? fileInfo->chan : 0;
    card_fire(callback, chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

// A real disk ID, not a stub: MemCard's constructor memcpy's straight out of
// it, and the save system keys on the game/company code. G4QE01 is the USA
// disc this decomp targets.
struct DVDDiskID *DVDGetCurrentDiskID()
{
    static DVDDiskID id = {};
    static bool filled = false;
    if (!filled) {
        id.gameName[0] = 'G';
        id.gameName[1] = '4';
        id.gameName[2] = 'Q';
        id.gameName[3] = 'E';
        id.company[0] = '0';
        id.company[1] = '1';
        filled = true;
    }
    return &id;
}
s32 DVDGetDriveStatus() { return 0; }
void DVDInit() {}

void GXCopyDisp(void *dest, GXBool clear) {}
void GXDrawDone(void) {}
void GXFlush(void) {}
void GXInvalidateTexAll(void) {}
void GXPeekARGB(u16 x, u16 y, u32 *color) {}
void GXPokeARGB(u16 x, u16 y, u32 color) {}
void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op) {}
void GXPokeColorUpdate(GXBool update_enable) {}
void GXSetDrawDone(void) {}
void GXWaitDrawDone(void) {}

void LCDisable(void) {}
void LCEnable(void) {}

void OSClearStack(u8 val) {}
BOOL OSDisableInterrupts(void) { return 0; }
BOOL OSEnableInterrupts(void) { return 0; }
u32 OSGetConsoleType(void)
{
    // MoviePlayerScene skips THP when (type & OS_CONSOLE_TDEV). No THP decode on
    // Vita yet — same skip ACGC effectively gets by not shipping GC movies.
    return OS_CONSOLE_TDEV;
}
u32 OSGetEuRgb60Mode(void) { return 0; }
unsigned char OSGetLanguage() { return 0; }
u32 OSGetProgressiveMode(void) { return 0; }
BOOL OSGetResetButtonState(void) { return 0; }
u32 OSGetSoundMode() { return 0; }
void OSReport(const char *, ...) {}
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {}
BOOL OSRestoreInterrupts(BOOL level) { return 0; }
OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) { return nullptr; }
void OSSetEuRgb60Mode(u32 on) {}
void OSSetProgressiveMode(u32 on) {}
void OSSetSoundMode(u32 mode) {}
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime *td) {}

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) {}

void SISetSamplingRate(u32 msec) {}

u32 THPAudioDecode(s16* audioBuffer, u8* audioFrame, s32 flag) { return 0; }
BOOL THPInit(void) { return 0; }
s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work) { return 0; }

void VIFlush(void) {}
u32 VIGetDTVStatus(void) { return 0; }
u32 VIGetTvFormat(void) { return 0; }
void VIInit(void) {}
void VISetBlack(BOOL black) {}
void VISetNextFrameBuffer(void *fb) {}
void VIWaitForRetrace(void) {}

} // extern "C"

// VM stubs (from dolphin/vm/VM.h)
extern "C" {
void VMAlloc(uintptr_t address, size_t size) {}
void VMInit(uintptr_t baseAddr, size_t initialCommitSize, uintptr_t limitAddr) {}
}
