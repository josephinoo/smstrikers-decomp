# PS Vita Port Iterative Boot Debugging

## Progress
The boot sequence has progressed through basic memory initialization and configuration loading. It successfully gets past `InterpreterCore` bytecode loading, basic platform stub initialization, and `AudioLoader` track initialization. It is currently crashing during the resource loading of the front end UI (`BootUI.Res`).

## Fixes Applied

1. **Crash in `nlBSearch` via `InterpreterCore.cpp`**
   - **Cause:** The bytecode files (`.byte_code`) are compiled for GameCube (Big-Endian). When parsed on Vita (Little-Endian), `ByteCodeHeader` fields like `numFunctions` were wildly large, causing out-of-bounds memory accesses when jumping to the `m_FunctionTable`.
   - **Fix:** Added `#ifdef TARGET_VITA` block in `InterpreterCore::LoadByteCode` to endian-swap `numFunctions`, segment sizes, `FunctionEntryPoint` table, `m_DataSegment`, and `m_CodeSegment`.

2. **Crash in `CreateBoxGeometry(PrimitiveShape&)` (ShapeRender.cpp)**
   - **Cause:** `glplatResourceAlloc` and `glplatFrameAlloc` inside `vita/src/plat/gx/glx_stubs.cpp` were returning `nullptr`, which was dereferenced when populating primitive positions/normals.
   - **Fix:** Implemented these allocator stubs to return `std::malloc(size)`. Also returned dummy objects (`glModel`, `GLSkinMesh`, `PlatTexture`) for other stubs returning `nullptr`.

3. **Crash in `AudioLoader::LoadFE(bool)`**
   - **Cause:** `g_pTrackManager->StopAllTracks(0)` was called on a `NULL` pointer. `g_pTrackManager` was not allocated in `audio.cpp:Initialize` because `PlatAudio::Initialize` (stub) was returning `false`.
   - **Fix:** Modified `PlatAudio::Initialize` and other sound group loading stubs in `vita/src/plat/plataudio_stubs.cpp` to return `true`. Also provided a dummy static `SFXEmitter` for functions returning one.

## Current Blocker
- **Crash:** Out-Of-Memory (OOM) `nlBreak()` in `MemoryAllocator::Allocate` at `src/NL/MemAlloc.cpp:219`.
- **Cause:** The engine requested an allocation of 603,979,776 bytes (`0x24000000`). This happens in `BundleFile::Open` when opening `art/fe/BootUI.Res`. `BundleFileHeader` is big-endian on disk. Reading `nNumFiles` (e.g. 0x00000024 = 36 files) on Vita makes it 0x24000000, causing a huge allocation request.
- **Next Step:** Implement an endian-swap for `BundleFileHeader` and `BundleFileDirectoryEntry` when reading `.Res` (BundleFile) headers inside `BundleFile::Open` in `src/NL/nlBundleFile.cpp`.

## Rendering Status
The game has not rendered anything to the screen yet. It has not reached the main game loop / front end drawing logic. No screenshots have been captured.
