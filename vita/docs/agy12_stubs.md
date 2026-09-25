# Link Stubs Digest (agy12_task)

## Summary
- **Dolphin SDK symbols undefined:** 0
- **MusyX symbols undefined:** 0
- **PlatAudio / GCAudioStreaming symbols undefined:** 0

## Files Created/Changed
- `vita/src/plat/dolphin_stubs.cpp` (new)
- `vita/src/plat/musyx_stubs.cpp` (new)
- `vita/src/plat/plataudio_stubs.cpp` (new)
- `vita/CMakeLists.txt` (modified to include the new sources in `SMS_PLAT_SOURCES`)

## Key Decisions
- Separated stubs into three logical files matching their groups.
- `VMInit` and `VMAlloc` were identified as Dolphin SDK OS symbols (Virtual Memory API) and stubbed alongside them.
- `PrintAvailableARAMMemory` was included in `plataudio_stubs.cpp` because it conceptually belongs with PlatAudio and its definition in `src/NL/plat/plataudio.cpp` is excluded from the Vita build alongside `PlatAudio::` itself.
- For `GCAudioStreaming` virtual functions (`~MonoAudioStream`, `Purge`, `SafeToPurge` for Mono/Stereo streams), they are declared `inline` in their respective headers. Providing non-inline out-of-line definitions doesn't force the compiler to emit them, which left them unresolved during link time. The solution was adding `__attribute__((used))` to their stub definitions in `plataudio_stubs.cpp` to force GCC to emit the symbols despite their header-inline status.

## Stubs That May Cause Misbehavior at Runtime
Returning neutral values (0, nullptr, false) for many hardware/platform layer APIs is dangerous and will likely cause issues if actually executed during game startup or normal operation:
1. **Memory Allocation:** `OSGetArenaHi`, `OSGetArenaLo` returning `nullptr` will likely cause the game's custom memory allocators to fail or crash immediately upon initialization. 
2. **Virtual Memory:** `VMInit` doing nothing and `VMAlloc` returning nothing will likely lead to memory mapping issues.
3. **Card/DVD Operations:** `CARDOpen`, `CARDMountAsync`, `CARDGetStatus`, `DVDGetCurrentDiskID` returning 0/nullptr will immediately fail any save/load or disc reading logic if the game doesn't handle the error gracefully. 
4. **MusyX / PlatAudio:** `sndStreamAllocEx` and `PlaySFX` returning 0 (often an invalid stream/voice ID) might cause subsequent `StopSFX` or parameter-setting functions to operate on an invalid ID, possibly leading to asserts or crashes in the MusyX engine if the stub implementations weren't bypassing it. `GetSFXEmitter` returning `nullptr` will likely lead to null dereferences when the game tries to manipulate the returned emitter.
