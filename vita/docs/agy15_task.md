# Task: iterate the Vita boot until the game reaches its front end

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp

This is a **long iterative debugging** task. Do not stop after one fix. Keep
going crash by crash until the game renders its front end, or until you hit
something you genuinely cannot resolve — and then say so plainly.

## The loop

```
# build
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita smstrikers-game.vpk-vpk -j8

# install + run
FS="$HOME/Library/Application Support/Vita3K/Vita3K/fs"
rm -f "$FS/ux0/data/smstrikers/_vita_fs.log"
unzip -o build/vita/smstrikers-game.vpk -d "$FS/ux0/app/VSTR00003" >/dev/null
(/Applications/Vita3K.app/Contents/MacOS/Vita3K -w -r VSTR00003 > /tmp/run.log 2>&1 &)
sleep 40
pkill -9 -f Vita3K          # ALWAYS. Never leave Vita3K running.

# locate the crash
PC=$(grep -oE 'PC: 0x[0-9a-f]+' /tmp/run.log | head -1 | cut -d' ' -f2)
arm-vita-eabi-addr2line -f -C -i -e build/vita/sms_entry $PC
```

Repeat: fix, rebuild, rerun, resolve the new address. Each iteration should get
further than the last. If two consecutive iterations land on the same line, stop
guessing and instrument instead.

## Where the boot is now

Passes: static initialisers, `nlInitMemory`, `MemCard` construction, all config
file I/O, `CrowdMood::ReadConfig`.

Current crash: `MemoryAllocator::Allocate` at `src/NL/MemAlloc.cpp:124`.

## Context you need

- `sms_entry` is the decomp's own `main()` (`-Dmain=sms_entry`) linked against
  the Vita platform layer in `vita/src/plat/`. It ships as VPK `VSTR00003`.
- The heap: `vita/src/plat/os/os_vita.cpp` hands the game a real 32 MB arena via
  `OSGetArenaLo/Hi`; `nlInitMemory` (src/NL/nlMemory.cpp) carves its heap out of
  it and memsets it to 0xCD. A wild pointer holding 0xCDCDCDCD means
  uninitialised heap.
- Tracing helper: `vita_fs_trace(const char* what, const char* detail)` in
  `vita/src/plat/dvd/fs_vita.cpp` appends to
  `ux0:data/smstrikers/_vita_fs.log`. Declare it at file scope (NOT inside a
  class) as `extern void vita_fs_trace(const char*, const char*);`. Read the log
  with `LC_ALL=C grep -a ... | LC_ALL=C tr -c '[:print:]\n' '.'` — it can
  contain non-UTF8 bytes.
- **Many remaining crashes will be the placeholder stubs.** `vita/src/plat/gx/glx_stubs.cpp`,
  `dolphin_stubs.cpp`, `musyx_stubs.cpp` and `plataudio_stubs.cpp` return
  `nullptr`/0 for things the engine dereferences: `glplatFrameAlloc`,
  `glplatResourceAlloc`, `glplatLoadModel`, `glx_CreatePlatTexture`,
  `glx_GetTex`, `glxGetBackBuffer`, `PlatAudio::GetSFXEmitter`,
  `sndStreamAllocEx`. When one of those is the cause, give it a **real minimal
  implementation** (allocate actual memory, return a real object), not another
  null.
- Rendering: the Vita renders through vitaGL. A working reference for what
  actually draws on Vita3K is `vita/src/plat/gx/vita_gxp_blit.cpp` — real
  geometry via `glVertexPointer`/`glColorPointer`/`glDrawArrays` with
  **three-component** positions. Vita3K's Vulkan renderer does NOT implement the
  GXM mask, so a scissored `glClear` wipes the whole framebuffer — never use
  that to draw. Keep `VITA_ENABLE_GXP=0`: the precompiled-GXP path crashes the
  Vita3K host process inside `glShaderBinary`/`glLinkProgram`.

## Hard rules

1. **Always `pkill -9 -f Vita3K` after every run.** Never leave it running.
2. Edits under `src/` or `include/` must be wrapped in `#ifndef TARGET_VITA` /
   `#ifdef TARGET_VITA` so the GameCube Metrowerks build stays byte-identical.
   Strongly prefer fixing things in `vita/src/plat/` instead. Never edit a
   shared header's types or signatures if a platform-layer fix will do.
3. Do not disable, skip or short-circuit game code to get past a crash. No
   commenting out calls, no early returns in engine functions, no clamping
   loop counters. Fix the cause.
4. Do not touch the `smsgame` GLOB or its exclusion filters in
   `vita/CMakeLists.txt`, and do not remove `-fno-short-enums` (the decomp
   assumes Metrowerks 4-byte enums; without it every struct layout shifts).
5. Remove your temporary instrumentation before you finish, or clearly mark it
   TEMPORARY. Do not revert instrumentation you did not add.
6. Report honestly. If a fix is a guess, say it is a guess.

## Definition of done

Best outcome: the game reaches `FrontEndTask` and draws something — report what
is on screen. Take a screenshot with `screencapture -x /tmp/shot.png` while it
is running, before you kill it.

Acceptable outcome: you got materially further and are blocked on something
specific. Report exactly how far the boot gets, the current crash with its
resolved `file:line`, what you tried, and what you ruled out with evidence.

Either way, write a digest to `vita/docs/agy15_boot.md` listing every crash you
fixed in order, with its cause and fix, so the next engineer can follow the
chain.
