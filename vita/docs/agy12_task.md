# Task: stub the Dolphin SDK / MusyX / PlatAudio link surface for sms_entry

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp

## Build & verify

```
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita sms_entry -j8
```

The full undefined-symbol list is in `vita/docs/link_undefined.md` (242 symbols,
grouped). Regenerate the live list at any time with:

```
make -C build/vita sms_entry -j8 2>&1 | grep -o "undefined reference to \`[^']*" | sed "s/.*to \`//" | sort -u
```

## Your scope — ONLY these three groups

1. **Dolphin SDK** (67 symbols): `OS*`, `DVD*`, `PAD*`, `CARD*`, `GX*`, `VI*`,
   `AR*`, `AI*`, `SI*`, `THP*`, `Mtx*`, `PSMTX*`, `LCEnable`/`LCDisable`, etc.
2. **MusyX audio** (10 symbols): `snd*`, `sal*`.
3. **PlatAudio::\*** (~45 symbols) plus `GCAudioStreaming::*`.

Everything else in that file — `glx*`, `glplat*`, `nl*`, `cPad*`, `cGlobalPad*`,
`PlatTexture::*`, `dl*`, `Detail::LexicalCastImpl*`, `GCTextureSize`,
`GetButtonIndex`, `InitPlatPad` — is **NOT yours**. Leave those undefined; I am
implementing them in parallel. Touching them will collide with my work.

## Hard rules

1. **Never edit anything under `src/` or `include/`.** These are the GameCube
   matching sources. All your code goes in new files under `vita/src/plat/`.
2. Do not modify `vita/CMakeLists.txt`'s `smsgame` GLOB or its exclusion filters.
   You MAY add your new source files to the `SMS_PLAT_SOURCES` list.
3. Signatures must match exactly what the linker asks for — demangle the symbol
   and read the real declaration in `include/` (e.g. `include/NL/plat/plataudio.h`,
   `extern/dolphin/include/`, `extern/musyx/include/`) before writing each stub.
   A wrong signature will not resolve the symbol.
4. Stub bodies do nothing and return a neutral value (0 / false / nullptr). Where
   a function must return a struct or a "valid" handle for the game not to
   immediately abort, return a zero-initialised value, and note it in the digest.
5. Put a short comment at the top of each new file saying what it stubs and that
   it is a stub, not an implementation.
6. Group the stubs into readable files, e.g.
   `vita/src/plat/dolphin_stubs.cpp`, `vita/src/plat/musyx_stubs.cpp`,
   `vita/src/plat/plataudio_stub.cpp`.

## Definition of done

`make -C build/vita sms_entry -j8` no longer reports ANY undefined reference from
your three groups. Symbols from the other groups will still be undefined and the
link will still fail overall — **that is expected and correct**; do not try to fix
them, and do not report it as failure.

Report: how many symbols from your groups remain undefined (should be 0), the
files you created, and any stub whose neutral return value is likely to make the
game misbehave at runtime. Write a digest to `vita/docs/agy12_stubs.md`.
