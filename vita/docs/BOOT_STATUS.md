# sms_entry boot status

`sms_entry` is the real game: the decomp's own `main()` (renamed via
`-Dmain=sms_entry`) linked against the Vita platform layer. It builds as its own
VPK (`VSTR00003`, "SM Strikers (game)") so a broken boot never costs us the
runnable demo shell (`VSTR00002`).

## Build

```
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita smstrikers-game.vpk-vpk -j8
```

517 objects compile; the link closes with zero undefined and zero duplicate
symbols.

## How to debug a boot crash

Vita3K reports only a register dump. Take the first `PC:` in the log and resolve
it against the unstripped ELF:

```
arm-vita-eabi-addr2line -f -C -e build/vita/sms_entry 0x<PC>
```

## Boot progress so far

| stage | result |
|---|---|
| static initialisers | pass |
| `nlInitMemory` | pass — needed a real 32 MB arena behind `OSGetArenaLo/Hi` |
| `MemCard` construction | pass — needed a real `DVDGetCurrentDiskID` |
| config file I/O | pass — `art/global.glt`, `common.ini`, `user.ini`, `audio/CrowdMood.ini` all open and parse |
| `CrowdMood::ReadConfig` | **fails here now** — see below |

Each crash is one more placeholder that needs real behaviour rather than a
neutral return. The two highest-risk groups, flagged when they were stubbed:

- `glplat*` / `glx*` allocators and object factories return `nullptr`
  (`glplatFrameAlloc`, `glplatResourceAlloc`, `glplatLoadModel`,
  `glx_CreatePlatTexture`, `glx_GetTex`, `glxGetBackBuffer`).
- `PlatAudio::GetSFXEmitter` / `GetFreeEmitter` return `nullptr`;
  `sndStreamAllocEx` / `PlaySFX` return 0 as a handle.

## Solved: enum ABI mismatch corrupted every decomp struct layout

`CrowdMood::ReadConfig` read back empty strings for every `_STRING` config
entry, so the `RandomHeckle` loop never terminated and walked off the end of
`g_RandomHeckles`.

Root cause: **Metrowerks on PowerPC gives every enum 4 bytes, and the decomp's
struct layouts assume that. ARM EABI defaults to `-fshort-enums`, which shrinks
enums to 1 byte and shifts those layouts.** The fix is one flag on both Vita
targets in `vita/CMakeLists.txt`:

```
-fno-short-enums
```

This is a whole class of latent bugs, not one struct: every `enum` field in
every decomp structure was potentially misplaced.

Verified by differential test — with the flag the boot passes `ReadConfig` and
reaches `MemoryAllocator::Allocate`; without it, it crashes back in
`CrowdMood.cpp` every time. Note that `sizeof(Config::TagValuePair)` is 12
either way (a `const char*` member forces 4-byte alignment, and the padding
absorbs the narrower enum), so measuring that struct alone does **not** reveal
the problem — the differential run does.

## Next blocker

`MemoryAllocator::Allocate` at `src/NL/MemAlloc.cpp:124`. Resolve the live
address yourself rather than trusting this one; it moves between builds.

## Known gaps

- `RenderSnapshot::Replay<LoadFrame/SaveFrame>` are empty specialisations in
  `vita/src/plat/game_gaps.cpp`: GCC refuses to instantiate the template from
  its header definition and we have not worked out why. Replay does nothing.
- `THPSimple.cpp:650` uses `BOOL*` on the Vita path where the GameCube used
  `int*`; `BOOL` is 1 byte here and 4 there. Latent bug in the movie player.
