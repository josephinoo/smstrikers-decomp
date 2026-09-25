# Task: stub the glx / glplat GX backend so sms_entry links

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp

## Build & verify

```
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita sms_entry -k -j8 2>&1 | grep -o "undefined reference to \`[^']*" | sed "s/.*to \`//" | sort -u
```

## Goal

The GameCube GX backend (`src/NL/glx/`) is excluded from the Vita build; the Vita
reimplements it over vitaGL. That real implementation is the next phase. **This
task is only to close the link** with correctly-typed placeholder definitions, so
we can boot the game and see how far `Initialize` gets.

## Your scope — ONLY these symbols

Every remaining undefined symbol whose name starts with `glplat`, `glPlat`, `glx_`,
`glx`, or `glSetMatrix` / `glSetIgnoreDuplicateModels`. Roughly 50 symbols.

NOT yours (I am doing these in parallel — do not define them):
`PSQUAT*`, `C_QUATSlerp`, `__construct_new_array`, `__cvt_fp2unsigned`,
`__float_min`, `__OSFpscrEnableBits`, `Detail::LexicalCastImpl*`,
`PlatTexture::*`, `StatsGatherer::*`, `SkinnedAnimController::*`,
`TMAnimController::*`, `cFuzzyDebugger::*`, `RenderSnapshot::Replay*`,
`sms_entry`.

## Hard rules

1. **Never edit anything under `src/` or `include/`** — those are the GameCube
   matching sources. New code goes in `vita/src/plat/gx/glx_stubs.cpp` (new file),
   added to `SMS_PLAT_SOURCES` in `vita/CMakeLists.txt`.
2. Do not modify the `smsgame` GLOB or its exclusion filters.
3. `vita/src/plat/gx/glplat_stub.cpp` already implements the startup/frame
   entry points (`glplatPreStartup`, `glplatStartup`, `glplatBeginFrame`, …).
   **Do not redefine anything it already defines** — you will get duplicate
   symbols. Read that file first.
4. Signatures must match the declarations in `include/NL/gl/glPlat.h`,
   `include/NL/glx/*.h` exactly. Include those headers and write the definitions
   against them rather than inventing prototypes.
5. Return neutral values (0 / false / nullptr). For functions returning a handle
   the engine dereferences, note it in the digest.
6. Head the file with a comment saying these are link placeholders for the GX
   backend, not an implementation, and that the real vitaGL backend replaces them.

## Definition of done

No symbol matching `glplat`, `glPlat`, `glx` remains undefined. Other groups will
still be undefined — expected, not a failure. Report the count that remains in
your scope (should be 0), and any placeholder likely to crash when the game
dereferences its result. Digest to `vita/docs/agy13_glx_stubs.md`.
