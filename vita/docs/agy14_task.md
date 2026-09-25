# Task: find what clobbers Config's string pool during CrowdMood::ReadConfig

Repo root: /Users/josephinoo/Documents/GitHub/smstrikers-decomp

This is a **debugging** task, not a stubbing task. Find the root cause first;
only then write the minimal fix.

## Build, install and run

```
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -C build/vita smstrikers-game.vpk-vpk -j8

FS="$HOME/Library/Application Support/Vita3K/Vita3K/fs"
rm -f "$FS/ux0/data/smstrikers/_vita_fs.log"
unzip -o build/vita/smstrikers-game.vpk -d "$FS/ux0/app/VSTR00003" >/dev/null
(/Applications/Vita3K.app/Contents/MacOS/Vita3K -w -r VSTR00003 > /tmp/run.log 2>&1 &)
sleep 40
pkill -9 -f Vita3K          # ALWAYS kill it; never leave Vita3K running
```

Read the trace (it can contain non-UTF8 bytes, so sanitise):

```
LC_ALL=C grep -a '^HECKLE' "$FS/ux0/data/smstrikers/_vita_fs.log" | LC_ALL=C tr -c '[:print:]\n' '.'
```

Resolve any crash address from /tmp/run.log:

```
PC=$(grep -oE 'PC: 0x[0-9a-f]+' /tmp/run.log | head -1 | cut -d' ' -f2)
arm-vita-eabi-addr2line -f -C -i -e build/vita/sms_entry $PC
```

## The symptom

`CrowdMood::ReadConfig` (src/Game/Audio/CrowdMood.cpp) reads back an **empty
string** for every `_STRING` entry. `sampleName` therefore never equals "none",
the `RandomHeckle` loop at ~line 700 never breaks, and it walks off the end of
`g_RandomHeckles`, faulting on reads of address 0.

## What the existing instrumentation already proves

Temporary tracing is already in the tree, all under `#ifdef TARGET_VITA`, in
`src/NL/nlConfig.cpp` (SET / STORE / FIND / POOL) and
`src/Game/Audio/CrowdMood.cpp` (HECKLE). It writes through `vita_fs_trace()` in
`vita/src/plat/dvd/fs_vita.cpp`. A run produces:

```
SET    RandomHeckle1 = [audio/.../HECKLER_Yell_Mono_01.dsp] STRING
STORE  RandomHeckle1 stored=[audio/.../HECKLER_Yell_Mono_01.dsp] slot=995 ptr=1859
POOL   audio/CrowdMood.ini poolUsed=3285 at1859=[audio/.../HECKLER_Yell_Mono_01.dsp]
FIND   RandomHeckle1 -> tag=RANDOMHECKLE1 type=3 slot=995 ptr=1859
HECKLE RandomHeckle1 sample=[] type=3 count=0
```

Same hash slot, same offset into `mStringMemory`, correct content immediately
after `Config::LoadFromFile` returns — and **empty** by the time the heckle loop
reads it, inside the same function. So something overwrites
`Config::mStringMemory` between the end of parsing and the heckle loop, i.e.
during the mood loop's `GetConfigFloat` / `Set(tag, "none")` calls.

## Already ruled out — do not re-investigate these

- **Not pool exhaustion.** The pool was enlarged from 0x2800 to 0x8000 for Vita
  (`NL_CONFIG_STRING_POOL` in src/NL/nlConfig.cpp) and the symptom is unchanged.
- **Not the file backend.** Every config file opens and parses; 205 tag/value
  pairs are classified correctly.
- **Not LexicalCast.** `LexicalCastImpl<const char*, const char*>::Do` was
  disassembled in the ELF and is a correct identity function.
- **Not a hash mismatch.** `Config::Hash` (used by `Set` via `FindConfigTvp`)
  and `ConfigHash` (used by `FindTvp`) are identical, and the traces confirm
  both land on slot 995.
- **Not a printf artefact.** The trace format was reordered to rule out
  `nlSNPrintf` mishandling varargs; result identical.

## The leading hypothesis (verify or disprove — do not assume)

`Config`'s string pool is allocated with `nlMalloc(..., atEnd=true)`
(`ALLOCATE_HIGH`), and `nlLoadEntireFile` allocates the file buffer with
`AllocateEnd` too. If the allocator's "allocate at end" path ever returns
overlapping blocks, then `nlFree(buffer)` inside `Config::LoadFileAsString`
would write free-list links over the pool. Look hard at
`MemoryAllocator::Allocate` / `Free` in `src/NL/MemAlloc.cpp` and at how
`nlInitMemory` in `src/NL/nlMemory.cpp` sizes the heap it is given.

The most direct next measurement: print `mStringEnd - mStringMemory` and the
pool base pointer at the top of the heckle loop, and compare against the `POOL`
line. If `mStringEnd` moved backwards, or the base pointer changed, that points
straight at the allocator.

## Hard rules

1. Changes to `src/` and `include/` must be wrapped in `#ifndef TARGET_VITA` /
   `#ifdef TARGET_VITA` so the GameCube Metrowerks path stays byte-identical.
   Prefer fixing this in `vita/src/plat/` if the root cause turns out to be in
   our platform layer.
2. **Always `pkill -9 -f Vita3K` after every run.** Never leave it running.
3. Do not "fix" this by skipping the heckle loop, clamping the counter, or
   making `ReadConfig` tolerate empty strings. That hides the bug. The strings
   must read back correctly.
4. You may add more `#ifdef TARGET_VITA` tracing; mark it TEMPORARY.

## Definition of done

Report the **root cause** with the evidence that proves it, and the minimal fix.
After the fix, a run must show `HECKLE RandomHeckle1 sample=[audio/...dsp]`
with the real path, and the heckle loop must terminate cleanly at the first
absent entry. Say how far the boot then gets (resolve the next crash address if
there is one). Write a digest to `vita/docs/agy14_config_pool.md`.

If you cannot find the root cause, say so plainly and report what you ruled out
and what you measured — do not invent a fix.
