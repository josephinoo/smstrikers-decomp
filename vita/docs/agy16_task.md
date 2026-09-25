# Task: byte-swap the GameCube asset loaders, then keep iterating the boot

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp

Long iterative task. Do not stop after one fix.

## The core problem

The data in `ux0:data/smstrikers/` is a **GameCube dump, so every binary file is
big-endian**. The Vita is little-endian. Every binary loader in the decomp must
byte-swap on load. Text files (`.ini`, `.txt`) are fine.

Confirmed, not inferred: `art/fe/BootUI.Res` starts `00 00 00 20` — that is 32
big-endian, but `0x20000000` (536 MB) little-endian. The current boot dies with
an OOM in `MemoryAllocator::Allocate` because a count read this way asks for
603979776 bytes.

`InterpreterCore::LoadByteCode` (`src/Game/InterpreterCore.cpp`) is already
swapped and `TARGET_VITA`-guarded — copy that shape.

## Immediate next step

`src/NL/nlBundleFile.cpp`: swap `BundleFileHeader` and every
`BundleFileDirectoryEntry` on load. Then keep going: the same treatment will be
needed for the `.Res`, `.glt`, model and animation loaders as the boot reaches
them.

## The loop

```
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita smstrikers-game.vpk-vpk -j8

FS="$HOME/Library/Application Support/Vita3K/Vita3K/fs"
rm -f "$FS/ux0/data/smstrikers/_vita_fs.log"
unzip -o build/vita/smstrikers-game.vpk -d "$FS/ux0/app/VSTR00003" >/dev/null
(/Applications/Vita3K.app/Contents/MacOS/Vita3K -w -r VSTR00003 > /tmp/run.log 2>&1 &)
sleep 40
pkill -9 -f Vita3K          # ALWAYS

PC=$(grep -oE 'PC: 0x[0-9a-f]+' /tmp/run.log | head -1 | cut -d' ' -f2)
arm-vita-eabi-addr2line -f -C -i -e build/vita/sms_entry $PC
```

Diagnostics already in the tree: an `OOM size=... align=...` line is appended to
`ux0:data/smstrikers/_vita_fs.log` on allocation failure. Read the log with
`LC_ALL=C grep -a ... | LC_ALL=C tr -c '[:print:]\n' '.'`.

**When a crash is a giant allocation, `xxd` the file being loaded** and check
whether a leading count reads sensibly big-endian before assuming anything else.

## Hard rules

1. **Always `pkill -9 -f Vita3K` after every run.** Never leave it running.
2. Every edit under `src/` or `include/` must be `#ifdef TARGET_VITA`-guarded so
   the GameCube Metrowerks build stays byte-identical — **including debug
   prints**. Check your own diff for unguarded lines before finishing; the last
   run left an unguarded `nlPrintf` and an unguarded `#include <cstdio>` in
   `src/NL/MemAlloc.cpp`.
3. Do not disable, skip or short-circuit game code. No commented-out calls, no
   early returns in engine functions, no clamped loop counters.
4. Do not touch the `smsgame` GLOB or its exclusion filters, and do not remove
   `-fno-short-enums`.
5. Do not revert instrumentation you did not add.
6. Swap in the loader, once, at load time. Do not scatter byte swaps through the
   consuming code, and make sure a buffer cannot be swapped twice.

## Definition of done

Best outcome: the game reaches `FrontEndTask` and draws something. Screenshot it
with `screencapture -x /tmp/shot.png` while it runs, before killing it, and say
what is on screen.

Otherwise: report exactly how far the boot gets, the current crash with resolved
`file:line`, and what you ruled out with evidence.

Digest to `vita/docs/agy16_endian.md`: every loader you swapped, and every crash
you fixed in order with its cause.
