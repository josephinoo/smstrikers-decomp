# Task: make the last 11 decomp TUs compile for PS Vita

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp
Build: `cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake`
       `make -C build/vita smsgame -k -j8`
Current state: 506 of 513 objects compile. The 11 files below still fail.

## Hard rules

1. **Do NOT break the GameCube matching build.** Never add `#ifdef VITA` / `#ifdef TARGET_VITA`
   to files under `src/` or `include/` unless there is genuinely no alternative, and if you do,
   guard so the Metrowerks path is byte-for-byte unchanged.
2. **Prefer Vita-side shims.** For "was not declared in this scope" errors, add the missing
   declaration to a Vita-only header (`vita/include/vita_mw_compat.h` is force-included into every
   TU via `-include`, and `vita/include/` is on the include path) rather than editing `src/`.
3. Where a shim needs a definition, put it in `vita/src/plat/link_stubs.cpp`.
4. Only compilation matters right now; linking is the next unit. Stubs may be empty bodies that
   return 0 / do nothing, but they must have the correct signature.
5. Do not delete or exclude any of these 11 files from the build.

## The failures


### src/Game/Sys/GCStream.cpp  (7 errors)
    37: conflicting declaration of C function 'void sndStreamMixParameterEx(long unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char)'
    38: conflicting declaration of C function 'void sndStreamDeactivate(long unsigned int)'
    39: conflicting declaration of C function 'void sndStreamFree(long unsigned int)'
    40: conflicting declaration of C function 'void sndStreamARAMUpdate(long unsigned int, long unsigned int, long unsigned int, long unsigned int, long unsigned int)'
    41: conflicting declaration of C function 'void sndStreamFrq(long unsigned int, long unsigned int)'
    42: conflicting declaration of C function 'void sndStreamADPCMParameter(long unsigned int, SND_ADPCMSTREAM_INFO*)'
    43: conflicting declaration of C function 'long unsigned int sndStreamAllocEx(unsigned char, void*, long unsigned int, long unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, long unsigned int, long unsigned int (*)(void*, long unsigned int, void*, long unsigned int, long unsigned int), long unsigned int, SND_ADPCMSTREAM_INFO*)'

### src/Game/Sys/PlatStream.cpp  (2 errors)
    8: conflicting declaration of C function 'void sndStreamMixParameterEx(long unsigned int, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char)'
    9: conflicting declaration of C function 'void sndStreamDeactivate(long unsigned int)'

### src/Game/Sys/THPSimple.cpp  (3 errors)
    15: conflicting declaration of C function 'void* memcpy(void*, const void*, long unsigned int)'
    16: conflicting declaration of C function 'void* memset(void*, int, long unsigned int)'
    650: cannot convert 'BOOL*' {aka 'bool*'} to 'int*' in assignment

### src/Game/Sys/gcmemcard.cpp  (6 errors)
    7: conflicting declaration of C function 'void* memset(void*, int, long unsigned int)'
    25: conflicting declaration of C function 'void* memset(void*, int, long unsigned int)'
    30: no declaration matches 's32 MemCard::BeginCardAccess(const MemCardFunctor&)'
    256: no declaration matches 's32 MemCard::FormatCard(const MemCardFunctor&)'
    405: no declaration matches 's32 MemCard::CloseFile(MC_FILE*)'
    420: no declaration matches 's32 MemCard::FileExists(const char*)'

### src/Game/Sys/simpleparser.cpp  (1 errors)
    77: '__ctype_map' was not declared in this scope; did you mean '_ctype_'?

### src/Game/Triggers/SebringAnimScript.cpp  (34 errors)
    53: 'u32* InterpreterCore::m_SP' is private within this context
    54: 'u32* InterpreterCore::m_SP' is private within this context
    55: 'u32* InterpreterCore::m_SP' is private within this context
    56: 'u32* InterpreterCore::m_SP' is private within this context
    57: 'u32* InterpreterCore::m_SP' is private within this context
    58: 'u32* InterpreterCore::m_SP' is private within this context
    64: 'u32* InterpreterCore::m_SP' is private within this context
    65: 'u32* InterpreterCore::m_SP' is private within this context

### src/Game/main.cpp  (4 errors)
    165: 'nlRegHandleDVDMessageCB' was not declared in this scope
    166: 'nlRegHandleDVDAllClearCB' was not declared in this scope
    167: 'nlRegCheckForResetFromFSCB' was not declared in this scope
    239: 'PADSetSamplingCallback' was not declared in this scope

### src/Game/world.cpp  (2 errors)
    930: '__ctype_map' was not declared in this scope; did you mean '_ctype_'?
    930: '__digit' was not declared in this scope; did you mean 'isdigit'?

### src/NL/gl/glState.cpp  (2 errors)
    446: ambiguating new declaration of 'long unsigned int glSetRasterState(eGLState, long unsigned int)'
    574: ambiguating new declaration of 'long unsigned int glSetCurrentTexture(long unsigned int, eGLTextureType)'

### src/NL/nlLocalization.cpp  (2 errors)
    6: array must be initialized with a brace-enclosed initializer
    7: array must be initialized with a brace-enclosed initializer

### src/NL/nlMemory.cpp  (1 errors)
    124: 'PADInit' was not declared in this scope; did you mean 'DVDInit'?

### vita/../include/Game/Sys/GCStreamVirtuals.h  (1 errors)
    18: 'nlFree' was not declared in this scope; did you mean 'nlFile'?
## Guidance per group

- **conflicting declaration of C function** (`sndStream*`, `memcpy`, `memset`): the decomp TU
  re-declares these with Metrowerks-era prototypes that clash with MusyX / newlib. Fix by making
  the declaration agree with the real one, not by deleting the call sites. Check what
  `extern/musyx/include` actually declares before changing anything.
- **`__ctype_map` / `__digit`** (simpleparser.cpp, world.cpp): MSL ctype internals. Provide
  newlib-backed equivalents in a Vita header so the call sites compile unchanged.
- **`m_SP` is private** (SebringAnimScript.cpp): look at `InterpreterCore` in `include/`; the
  shipped build clearly allowed this access. Widen access or add the friend declaration the
  Metrowerks build implied. Do not rewrite the call sites.
- **`no declaration matches MemCard::*`** (gcmemcard.cpp): the header and the .cpp disagree on
  signatures. Reconcile them; the .cpp is the authority on what the game calls.
- **`ambiguating new declaration`** (glState.cpp): return type mismatch between header and .cpp.
- **array must be initialized with a brace-enclosed initializer** (nlLocalization.cpp:6-7): look
  at the actual declaration; MW allowed a form GCC does not.
- **`nlRegHandleDVDMessageCB` / `nlRegHandleDVDAllClearCB` / `nlRegCheckForResetFromFSCB`**
  (main.cpp) and **`PADInit` / `PADSetSamplingCallback`** (nlMemory.cpp, main.cpp): GameCube DVD
  and PAD entry points. Declare them Vita-side and stub them in `vita/src/plat/link_stubs.cpp`;
  the real pad implementation already exists in `vita/src/plat/pad/pad_vita.cpp` — wire to it if
  the signature matches, otherwise stub.
- **`nlFree`** (include/Game/Sys/GCStreamVirtuals.h:18): missing declaration; find where `nlFree`
  is really declared and include it, or declare it Vita-side.

## Definition of done

`make -C build/vita smsgame -k -j8` builds **513 of 513** objects with zero errors.
Report the exact object count and any file you could not fix, with its remaining error text.
Write a digest to `vita/docs/agy11_compile_fixes.md` listing every file you touched and why.
