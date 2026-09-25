# Unit 5/10: NL-Facing File Shim (`nl_fs_shim`)

## Overview

Unit 5 implements the Next Level Games (`NL`) file abstraction layer for the Super Mario Strikers PS Vita port. It connects high-level engine consumers—primarily `Config::LoadFromFile` (`nlLoadEntireFile`, `nlFlushFileCash`, `nlFree`) and `glLoadTextureBundle` (`nlOpen`, `nlRead`, `nlSeek`, `nlClose`)—to the Vita platform virtual filesystem (`vita_file_*`) under `VITA_DATA_ROOT` (`ux0:data/smstrikers`).

All implementations reside strictly within `vita/src/plat/dvd/nl_fs_shim.cpp` and `vita/include/`, leaving original GameCube engine sources (`src/NL/nlFileGC.cpp`, `src/NL/nlFile.cpp`, `include/NL/nlFileGC.h`) untouched.

---

## 1. Files Changed / Created

- `vita/src/plat/dvd/nl_fs_shim.cpp` (created):
  Core NL file shim implementing `nlFile`, `GCFile`, `VitaFile`, `nlOpen`, `nlClose`, `nlRead`, `nlSeek`, `nlFileSize`, `nlGetFilePosition`, `nlFlushFileCash`, `nlInitFileSystem`, `nlServiceFileSystem`, `nlLoadEntireFile`, `nlLoadEntireFileToVirtualMemory`, `nlReadAsync`, and weak memory helpers (`nlMalloc`, `nlFree`).
- `vita/src/plat/dvd/fs_vita.cpp` (modified):
  Extended Vita VFS with `vita_path_resolve`, `vita_file_seek`, `vita_file_tell`, `vita_fs_fatal_missing_common_ini`, and `vita_ensure_data_or_fatal`.
- `vita/include/plat_abi.h` (modified):
  Declared extended VFS primitives (`vita_path_resolve`, `vita_file_seek`, `vita_file_tell`) and early fatal helpers (`vita_fs_fatal_missing_common_ini`, `vita_ensure_data_or_fatal`).
- `vita/include/NL/nlFileGC.h` (created/refined):
  Vita-target replacement header providing the minimal `GCFile` class definition and C++ prototypes without pulling in GameCube DVD headers (`dolphin/dvd.h`, `TDEVChunkFile`, `DolphinFile`).
- `vita/include/NL/nlMemory.h` (created/refined):
  Minimal memory operator and allocator declarations (`nlMalloc`, `nlFree`, placement/aligned `new` operators) matching NL memory conventions.
- `vita/CMakeLists.txt` (modified):
  Added `src/plat/dvd/nl_fs_shim.cpp` to `smstrikers_vita` target sources.
- `vita/docs/agy5_nl_fs.md` (created):
  This documentation digest.

---

## 2. Symbols Exported (with Signatures)

### 2.1 Core NL File Interface (`include/NL/nlFile.h` / `vita/include/NL/nlFileGC.h`)

```cpp
// Opening / closing handles
nlFile* nlOpen(const char* fileName);
void nlClose(nlFile* file);

// Synchronous I/O and positioning
void nlRead(nlFile* file, void* buffer, unsigned int size);
void nlSeek(nlFile* file, unsigned int offset, unsigned long origin);
unsigned int nlFileSize(nlFile* file, unsigned int* size);
u32 nlGetFilePosition(nlFile* file);

// Entire file loaders
void* nlLoadEntireFile(const char* filename, unsigned long* outSize, unsigned int alignment, eAllocType type);
void* nlLoadEntireFileToVirtualMemory(const char* fileName, int* size, unsigned int transferSize, void* target, eAllocType allocType);

// Async I/O stubs (synchronous completion on Vita)
bool nlLoadEntireFileAsync(const char* filename, LoadAsyncCallback callback, void* user_data, unsigned int alignment, eAllocType type);
void nlReadAsync(nlFile* file, void* buffer, unsigned int size, ReadAsyncCallback callback, unsigned long uParam);
bool nlAsyncReadsPending(nlFile* file);
void nlCancelPendingAsyncReads(nlFile* pFile, void (*callback)(nlFile*, void*, unsigned int, unsigned long, void (*)(nlFile*, void*, unsigned int, unsigned long)));

// Filesystem lifecycle stubs
void nlFlushFileCash();
void nlInitFileSystem();
void nlServiceFileSystem();

// Polymorphic base class methods (weak symbols)
nlFile::nlFile();
virtual nlFile::~nlFile();
```

### 2.2 Memory Management Helpers (Weak Symbols)

```cpp
void* nlMalloc(unsigned long size, unsigned int alignment, bool atEnd);
void* nlMalloc(unsigned long size);
void nlFree(void* ptr);
```

### 2.3 Platform VFS Layer (`vita/include/plat_abi.h`)

```c
bool vita_path_resolve(const char* relative_path, char* out_path, size_t out_len);
void* vita_file_open(const char* relative_path);
void vita_file_close(void* file);
size_t vita_file_read(void* file, void* buf, size_t n);
long vita_file_size(void* file);
int vita_file_seek(void* file, long offset, int whence);
long vita_file_tell(void* file);
bool vita_data_present(void);
bool p0_mount_data(void);

// Fatal diagnostics
void vita_fs_fatal_missing_common_ini(void);
bool vita_ensure_data_or_fatal(void);
```

---

## 3. Path Mapping Rules

Path resolution in `vita_path_resolve` enforces strict sandboxing within `VITA_DATA_ROOT` (`ux0:data/smstrikers`):

1. **Root Redundancy Stripping**: If a caller passes a path already prefixed with `ux0:data/smstrikers/`, the prefix is trimmed.
2. **Leading Separator Normalization**: Any leading slashes or backslashes (`/` or `\\`) are stripped, converting `/art/global.glt` to `art/global.glt`.
3. **Subdirectory Traversal Guards**: Any path containing `..` parent references or empty paths after trimming are rejected (`returns false`), preventing directory escape.
4. **VFS Root Prefixing**: Relative paths are prefixed with `ux0:data/smstrikers/%s`.
   - `"art/global.glt"` $\rightarrow$ `"ux0:data/smstrikers/art/global.glt"`
   - `"/common.ini"` $\rightarrow$ `"ux0:data/smstrikers/common.ini"`
   - `"common.ini"` $\rightarrow$ `"ux0:data/smstrikers/common.ini"`

---

## 4. Key Decisions

1. **Wrapping Platform Primitives without Touching GCN Code**:
   `nl_fs_shim.cpp` implements `nlFile` via an internal `VitaFile` subclass derived from `GCFile`. `VitaFile` delegates all physical I/O directly to `vita_file_read`, `vita_file_seek`, `vita_file_tell`, and `vita_file_close`. This reuses the platform VFS and prevents code duplication.

2. **GameCube Seek Origin Semantics**:
   The GCN implementation of `nlSeek` used:
   - `0`: `SEEK_SET`
   - `1`: `SEEK_CUR`
   - `2`: `FileSize - offset` (relative to end)
   `VitaFile::Seek` matches this exact mapping, computing `(sz >= offset ? sz - offset : 0)` for origin `2`, ensuring full compatibility with engine loaders like `glplatLoadTextureBundle`.

3. **Weak Base Class and Allocator Symbols**:
   `nlFile::nlFile()`, `nlFile::~nlFile()`, `nlMalloc()`, and `nlFree()` are declared with `__attribute__((weak))`. This allows standalone linking of `smstrikers_vita` while permitting other units or future memory subsystems (`nlMemory.cpp`) to override them seamlessly without multiple-definition collisions.

4. **Early Missing `common.ini` Fatal Handling**:
   - `nlOpen("common.ini")` checks if the handle is null. If missing, it immediately invokes `vita_fs_fatal_missing_common_ini()`.
   - `vita_fs_fatal_missing_common_ini()` emits descriptive diagnostics to both `stdout` and `stderr`.
   - If VitaGL has been initialized (`glplatIsInitialized() == true`), it clears the framebuffer to an unmistakable bright red screen (`RGB(0.85, 0.05, 0.05)`) and flips the display with `vglSwapBuffers(GL_FALSE)`, making missing asset errors instantly visible on physical hardware and emulator screens.

5. **Header Shadowing under `vita/include`**:
   `vita/CMakeLists.txt` places `vita/include` ahead of `../include`. Providing `vita/include/NL/nlFileGC.h` ensures that references to `<NL/nlFileGC.h>` in engine code compile against the clean Vita ABI rather than the GameCube-specific hardware DVD headers.

---

## 5. Verification

- **Compilation**: Successfully compiled with `arm-vita-eabi-g++` (C++17, `-Wall -Wextra -Wpedantic`). Zero compiler warnings, zero errors.
- **Linker Inspection**:
  - `_Z6nlOpenPKc` (`nlOpen`)
  - `_Z7nlCloseP6nlFile` (`nlClose`)
  - `_Z6nlReadP6nlFilePvj` (`nlRead`)
  - `_Z6nlSeekP6nlFilejm` (`nlSeek`)
  - `_Z16nlLoadEntireFilePKcPmj10eAllocType` (`nlLoadEntireFile`)
  - All symbols exported as strong text symbols (`T`).
- **Target Generation**: Built `smstrikers_vita`, `smstrikers_vita.velf`, `eboot.bin`, and `smstrikers-vita.vpk`.

---

## 6. Context for Next Step (Units 6+)

With `nl_fs_shim.cpp` providing the complete synchronous NL file I/O ABI, subsequent units can implement the real `Config::LoadFromFile` parser (Unit 6: INI dictionary decoding) and real texture bundle decompression (Unit 6/7: `glplatLoadTextureBundle` with `glx_FreeMemory` and GXP/texture upload) without encountering missing file system symbols or GameCube DVD dependencies.
