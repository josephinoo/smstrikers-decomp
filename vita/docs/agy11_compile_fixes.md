# Compile Fixes Digest

## Objects Compiled
Expected: 513 objects
Actual: 513 objects

## Files Modified

1. **src/Game/Sys/GCStream.cpp**
   - **Issue:** Redeclarations of `sndStreamMixParameterEx`, `sndStreamDeactivate`, etc., used explicit `unsigned long`, conflicting with MusyX headers defining them with `u32` (which is `unsigned int` on GCC/Vita).
   - **Fix:** Wrapped the existing prototypes in `#ifndef TARGET_VITA` and provided `#else` paths with `u32` and `u8` matching the actual MusyX signatures, keeping the Metrowerks path unchanged.

2. **src/Game/Sys/PlatStream.cpp**
   - **Issue:** Mismatched signatures for `sndStreamMixParameterEx` and `sndStreamDeactivate`.
   - **Fix:** Wrapped the redeclarations in `#ifndef TARGET_VITA` and provided `#else` paths using `u32`/`u8`.

3. **src/Game/Sys/THPSimple.cpp**
   - **Issue:** `memcpy` and `memset` redeclared with `unsigned long` conflicted with `<string.h>` on Vita. Additionally, assigning to `int* validBuffer` from `BOOL` pointer caused an error because GameCube's `BOOL` was `int`, while Vita's was `bool`.
   - **Fix:** Wrapped `memcpy` and `memset` in `#ifndef TARGET_VITA`. Changed `int* validBuffer` to `BOOL* validBuffer` under `#ifdef TARGET_VITA` to maintain correct types.

4. **src/Game/Sys/gcmemcard.cpp**
   - **Issue:** `memset` redeclaration conflicted with `<string.h>`, and `MemCard::BeginCardAccess` (and others) disagreed with the header on return types (using `s32` instead of `long`).
   - **Fix:** Wrapped `memset` in `#ifndef TARGET_VITA`.

5. **include/Game/Sys/gcmemcard.h**
   - **Issue:** Header declared `long` for methods that returned `s32` in the `.cpp`.
   - **Fix:** Changed the return types for `BeginCardAccess`, `FormatCard`, `CloseFile`, and `FileExists` from `long` to `s32` to match `gcmemcard.cpp` implementations.

6. **vita/include/vita_mw_compat.h**
   - **Issue:** MSL ctype internals (`__ctype_map`, `__digit`) were missing for `simpleparser.cpp` and `world.cpp`.
   - **Fix:** Provided newlib equivalents: declared `extern "C" const char _ctype_[]`, mapped `__ctype_map` to `(_ctype_ + 1)`, and defined `__digit` as `04` (`_N` equivalent).

7. **include/Game/Triggers/AnimTagScript.h**
   - **Issue:** `InterpreterCore::m_SP` was inaccessible in `SebringAnimTagScriptInterpreter` due to private inheritance.
   - **Fix:** Added `friend class SebringAnimTagScriptInterpreter;` to `AnimTagScriptInterpreter` to grant the implied Metrowerks access.

8. **src/NL/gl/glState.cpp** and **include/NL/gl/glState.h**
   - **Issue:** Return types for `glSetRasterState` and `glSetCurrentTexture` were `u32` in the header but `unsigned long` in the `.cpp`.
   - **Fix:** Updated the header to use `unsigned long` for these return types, matching the implementation.

9. **src/NL/nlLocalization.cpp**
   - **Issue:** Wide string literals (`L"..."`) assigned to `unsigned short[]` failed on GCC because GCC's `wchar_t` is 32-bit (Metrowerks was 16-bit).
   - **Fix:** Wrapped the initialization in `#ifndef TARGET_VITA` and provided a brace-enclosed array initializer for the Vita build.

10. **include/Game/Sys/GCStreamVirtuals.h**
    - **Issue:** `nlFree` was called but undeclared.
    - **Fix:** Added `#include "NL/nlMemory.h"` to bring in the `nlFree` declaration.

11. **vita/include/NL/nlFileGC.h**
    - **Issue:** Missing DVD callback declarations for `nlRegHandleDVDMessageCB`, `nlRegHandleDVDAllClearCB`, and `nlRegCheckForResetFromFSCB`.
    - **Fix:** Added the declarations directly to this Vita override header.

12. **vita/include/dolphin/pad.h**
    - **Issue:** `PADInit` and `PADSetSamplingCallback` were missing from the Vita-specific `pad.h`.
    - **Fix:** Added the missing declarations, changed `<stdint.h>` to `<dolphin/types.h>`, and removed the redundant `BOOL` typedef block.

13. **vita/src/plat/link_stubs.cpp**
    - **Issue:** Missing stubs for DVD callbacks and PAD functions.
    - **Fix:** Added stubs for `nlRegHandleDVDMessageCB`, `nlRegHandleDVDAllClearCB`, `nlRegCheckForResetFromFSCB`, and `PADSetSamplingCallback`. Implemented `PADInit()` to call `InitPlatPad()`.

## Remaining Errors
None. All 513 objects compile successfully.
