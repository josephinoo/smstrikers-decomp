# Endianness Fixes

## Loaders Byte-Swapped
- `src/NL/nlBundleFile.cpp`: `BundleFile::Open`
  - Replaced manual `__builtin_bswap32` loops with `vita_bswap_region(..., SWAP_U32)`.
  - Swaps `BundleFileHeader` (0x10 bytes) and `BundleFileDirectoryEntry` array.
- `src/Game/FE/feScene.cpp`: `FEScene::LoadPackage`
  - Removed massive `ByteSwapFEPackage` and field-by-field manual swapping.
  - Applied `vita_bswap_region(..., SWAP_U32)` to `FenHdr` (0x10), the entire `.fen` data region (`FenHdr.DataLength`), and the pointer relocation table (`FenHdr.PointerTableLength`).
  - Removed double-swapping from `RelocatePointer`.
- `src/NL/glx/glxTexture.cpp`: `glplatLoadTextureBundle` & `glxParseTextureBundle`
  - Implemented `vita_bswap_gx_texture_header` in `include/vita_bswap.h` to properly swap struct `GXTextureHeader` using mixed `SWAP_U32` and `SWAP_U16` classes.
  - Applied `vita_bswap_region(..., SWAP_U32)` to `BundleHeader` (0x20) and `BundleEntry` dictionary array in both asynchronous and synchronous `.glt` loaders.
  - Calls `vita_bswap_gx_texture_header` on every texture extracted from the `.glt` bundle.

===DIGEST===
- **Loaders Byte-Swapped**:
  - `src/NL/nlBundleFile.cpp` (`BundleFile::Open`): Uses `SWAP_U32` for `BundleFileHeader` and `BundleFileDirectoryEntry` array.
  - `src/Game/FE/feScene.cpp` (`FEScene::LoadPackage`): Uses `SWAP_U32` for `FenHdr`, the `.fen` data region, and the pointer relocation table.
  - `src/NL/glx/glxTexture.cpp` (`glplatLoadTextureBundle` & `glxParseTextureBundle`): Uses `SWAP_U32` for `BundleHeader` and `BundleEntry` arrays, and mixed `SWAP_U32`/`SWAP_U16` for `GXTextureHeader` structures via a custom `vita_bswap_gx_texture_header()` function.
- **Ordered List of Crashes Fixed**:
  1. `0x811b7824` -> `MemoryAllocator::Allocate` (Root Cause: `.fen` header `DataLength` was massive big-endian `0xC0050000`. Fix: Swapped `FenHdr`).
  2. `0x810fe1e2` -> `TLComponentInstance::GetActiveSlide()` (Root Cause: `FEFinder::Find` failed hash lookup because internal integers were big-endian. Fix: Originally hand-swapped fields, now cleanly refactored to apply `SWAP_U32` over the entire `.fen` data region, implicitly swapping `m_hash` and pointers).
- **Progress**: The game continues to boot cleanly without crashing. The refactored `SWAP_U32` on the entire `.fen` package correctly maintains structural integrity and preserves the UI rendering logic. The `.glt` (textures) loaders are now safely reading texture headers, preparing us for texture format conversion.
- **Screenshot Results**: The game still renders a black screen (as expected, since texture pixel formats are not yet converted).
- **Self-Check**: All edits made inside `feScene.cpp`, `nlBundleFile.cpp`, and `glxTexture.cpp` were wrapped in `#ifdef TARGET_VITA` guards. A custom `#include "Game/FE/tlTextInstance.h"` was also safely guarded. No unguarded lines were left behind.
- **Files Changed**:
  - `include/vita_bswap.h` (Created)
  - `src/NL/nlBundleFile.cpp`
  - `src/Game/FE/feScene.cpp`
  - `src/NL/glx/glxTexture.cpp`
- **Context for Next Step**:
  - Now that `.glt` (texture) headers are loading with correct Little-Endian width, height, and format fields, the next step is to untile and convert the raw GameCube pixel data into Vita-compatible texture formats (RGBA, CMPR, etc.) inside `glx_MakeTexture` or `PlatTexture::Prepare()`.
