# SM Strikers — PS Vita full port plan

LLM-executable phased plan. Each phase is self-contained: docs → implement → verify.
Do **not** touch GameCube matching (`configure.py` / `src/Dolphin` / Metrowerks). Port lives under `vita/`.

**Goal:** Playable match on PS Vita / PSTV from a legally owned GameCube dump + this decomp’s portable `Game`/`NL`/`ode` sources.

**Non-goals:** Byte-matching DOL, assets in git/VPK, `#ifdef VITA` pollution of matching GCN files, 60fps lock.

---

## Phase 0 — Documentation Discovery (done baseline)

### Allowed APIs already in-repo (copy these patterns)

| Area | Source | Copy |
|------|--------|------|
| Vita CMake / VPK | `vita/CMakeLists.txt` | `vita.cmake`, `vita_create_self`, `vita_create_vpk`, link set (`vitaGL`, `SceCtrl_stub`, …) |
| Bootstrap build | `vita/README.md` L9–12 | `cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake` |
| Data contract | `vita/README.md` L19–33 | `ux0:data/smstrikers/` + probe `common.ini`; no assets in VPK |
| Runtime loop skeleton | `vita/src/main.cpp` | `vglInit` → `sceCtrlPeekBufferPositive` → `glClear` → `vglSwapBuffers` |
| Boot order (GC) | `src/Game/main.cpp` `Initialize` + `main` L163–490 | DVD → `nlInit` → `glStartup` → pads → tasks |
| NL entry | `src/NL/nlMain.cpp` L15–22 | `nlInitMemory` → `glplatPreStartup` → ticker/random/FS |
| GL facade → plat | `src/NL/gl/gl.cpp` L98–219 | `glplatBeginFrame/End/Send/Finish`, `glplatStartup` |
| `glplat*` ABI | `include/NL/gl/glPlat.h` L43–64 | Must implement on Vita |
| Texture plat | `include/NL/glx/glxTexture.h` L161–179 | `glplatTexture*` / bundle load |
| Swap | `include/NL/glx/glxSwap.h` L4–13 | `glxInitSwap`, `glxSwapPre/Post` |
| Pad ABI | `include/NL/platpad.h` | `InitPlatPad`, `UpdatePlatPad`, `VBlankPadUpdate`, `cPlatPad` |
| Audio ABI | `include/NL/plat/plataudio.h` | `PlatAudio::*` (MusyX today) |
| Math ABI | `include/NL/platvmath.h`, `platqmath.h` | Soft C++/NEON; **no** PPC `psq_*` |
| Files | `include/NL/nlFile.h`, `nlFileGC.h`, `nlBundleFile.h` | Replace DVD backend; keep `nlOpen`/`nlBundle` |
| Endian helper | `src/NL/nlEndian.cpp` | Extend for asset pipeline (offline preferred) |

### External Allowed APIs (verify against installed VitaSDK / VitaGL before coding)

Verified locally under `$VITASDK/arm-vita-eabi/include` + `$VITASDK/share/vita.cmake` (cmake macros OK; headers live under `arm-vita-eabi/`, not `$VITASDK/include`).

- **CMake:** `vita.toolchain.cmake`, `include(vita.cmake)`, `vita_create_self` / `vita_create_vpk` (already in `vita/CMakeLists.txt`); optional `vita_create_stubs`
- **VitaGL:** `vglInit` / `vglInitExtended`, `vglSwapBuffers`, `vglWaitVblankStart`, `vglUseTripleBuffering`, `vglDrawObjects`, plus GLES-like `glClear*` / `glDrawArrays|Elements` / VBO / textures / programs — **no** `vglEnd` / `vglDeinit` (exit via `sceKernelExitProcess`)
- **Ctrl:** `sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG)`, `sceCtrlPeekBufferPositive` / `ReadBufferPositive`, `SCE_CTRL_*` (incl. L1/R1/LTRIGGER/RTRIGGER aliases), analogs `lx/ly/rx/ry` center 128
- **FS:** `app0:` RO VPK; data at `ux0:data/smstrikers/`; saves preferably `ux0:user/00/savedata/<TITLE_ID>/`; `fopen` with drive prefix **or** `sceIo*`
- **Audio (decided preference):** `SceAudio_stub` + `sceAudioOutOpenPort` → `SetVolume` → `Output` → `ReleasePort` (PCM thread). Prefer over NGS for raw stream. Link `SceAudio_stub` when Phase 8 starts
- **Shaders:** Precompiled GXP binaries embedded into C headers (`vita_gxp_shaders.h` / `gxp_simple.h`) via Sony `psp2cgc`. When GXP is committed in the repository, `libshacccg.suprx` / runtime shacc is **NOT** required for day-to-day compilation or running on Vita / Vita3K (VitaGL loads GXP blobs directly via `glShaderBinary`).
- **GX→GL prior art:** **[ACGC-Vita-Port](https://github.com/Brendonm17/ACGC-Vita-Port)** (vitaGL VPK `ac_vita.vpk`) + upstream [ACGC-PC-Port](https://github.com/flyngmt/ACGC-PC-Port) `pc/src/pc_gx*.c` / `pc_gx_texture.c` / `pc_disc.c`. Note: ACGC Vita port lives on branch **`vita`** (clone with `git clone -b vita ...`; `master` is PC). Do not invent GX APIs outside `include/NL/glx/*`.

### Anti-patterns

- Editing matching GCN sources for Vita quirks
- Linking `src/Dolphin` or `PowerPC_EABI_Support` into Vita
- VitaGL / fake GX immediate mode (`glBegin`/`glVertex` per FIFO vert); GX batches end at `nverts` (`GXBegin`), not a separate `GXEnd` API to port
- Invented Vita APIs: `vglEnd`, `vglDeinit`, `sceCtrlInit`
- PSP leftovers: `sceGu*`, PSP audio, single-stick assumptions
- Shipping ISO/DOL/assets in repo or VPK
- Assuming 60fps with full AI + crowd + MusyX software mix
- Inventing GX/TEV helpers that are not in `include/NL/glx/*` ABI

### Constraints (from research)

- Target **30fps** first
- Force widescreen; screen **960×544** (GC hardcodes 640×448 in `glPlat.cpp` / `glxSend.cpp`)
- Endian: GC BE assets → offline byteswap pipeline preferred over runtime
- Keep decomp CI (`.github/workflows/build.yml`) untouched; Vita CI is a **separate** workflow later
- Supported dump for Vita data: **G4QE01** (USA) — matches `configure.py` `DEFAULT_VERSION`
- Build dir: use `build/vita` (README); `.gitignore` also lists `vita-build/` — both OK, prefer README path

### Discovery provenance (Phase 0)

| Slice | Agent | Absorbed into |
|-------|-------|---------------|
| Bootstrap/CI/GC build surface | [Vita bootstrap docs](a9adb7a3-cd8a-4b5a-acac-3d052beabfbb) | Allowed APIs table + CI non-goals |
| NL `glplat*`/`plat`/`nlFile` ABI | [NL plat/glx surface](b2e9c5b8-48b1-49f2-91f0-2e253bff5aa1) | Architecture + Phase 1 stub list |
| OS/DVD/PAD/ARAM/MusyX blockers | [Platform layer DIGEST](eabd878d-c980-4935-a7ed-412aaa004e44) | Phases 1–2, 8 + blockers |
| VitaSDK/VitaGL/SceCtrl/AudioOut | [VitaSDK Allowed APIs](2452c6c4-5805-4160-a2e7-6a61af15a0a1) | External Allowed APIs + anti-patterns; AudioOut preferred |

**Extra stub pressure beyond NL** (Game): `CARD*`, `SI*`, `THPSimple`/movie, `ResetTask`/`DVDGetDriveStatus`, `gcmemcard` — stub in Phase 1–3, not first rewrite.

**Keep portable first:** most `src/NL/gl/gl{View,State,Draw*,Model,Memory,Font,Constant,Modify,UserData,RenderList}.cpp`, `nlBundleFile.cpp`, `nlEndian.cpp`.

### Reference: ACGC Vita branch (the real game lives here)

Repo: [Brendonm17/ACGC-Vita-Port](https://github.com/Brendonm17/ACGC-Vita-Port) (local reference clone: `/tmp/ACGC-Vita-Port/`).
**IMPORTANT:** Always clone using branch **`vita`** (NOT `master`, which is PC):

```bash
git clone -b vita https://github.com/Brendonm17/ACGC-Vita-Port.git
```

Playable architecture under `vita/`: `vita/src/vita_main.c` → ROM/assets at `ux0:data/AnimalCrossing/rom/` → `ac_entry`/`boot_main`. Graphics = shared `pc/src/pc_gx*.c` + Vita cmdbuf/frame worker (`vita/src/vita_gx_cmdbuf.c`, `vita/src/vita_frame.c`) + **precompiled GXP** (`vita/include/vita_gxp_shaders.h`, generated by `vita/shaders/compile_new_shaders.sh` and `vita/shaders/gen_header.py`). Day-to-day builds need **no** `libshacccg.suprx` or `psp2cgc` because GXP headers are committed. VitaGL fork: `Brendonm17/vitaGL` `@async-compressed-tex-prep`.

| ACGC `vita/` pattern & file path | SMS action |
|-----------------------------------|------------|
| `vita/shaders/compile_new_shaders.sh` + `vita/shaders/gen_header.py` → `vita/include/vita_gxp_shaders.h` | Precompile `.cg` shaders with `psp2cgc` and commit embedded GXP headers (`gxp_simple.h` / `vita_gxp_shaders.h`). Day-to-day `shacc` / `libshacccg` is **NOT required**. |
| `pc/src/pc_gx_tev.c:680-699` (`vita_load_gxp`) & `pc/src/pc_gx_tev.c:158` (`vita_get_simple_shader`) | Load GXP blobs directly via `glShaderBinary(1, &shader, 0, buf, total)` with a 4-byte `matrix_uniforms_num = 0` header prepended. |
| `vita/src/vita_banner.c:108-118, 149-230` (`banner_draw_bars`) | Render textured quads via VBO + `glDrawArrays` with simple GXP shader. **Never use scissor-clear-only hacks** for title/UI graphics. |
| `pc/src/pc_gx_texture.c:1322-1660` (`decode_gc_texture`, `decode_RGB5A3`, `decode_CMPR`, `decode_CI4`/`CI8`, `decode_RGBA8`) | Port GameCube texture decoders into `vita/src/plat/gx/` for `opening.bnr` (RGB5A3) and FE assets (CMPR/CI8/RGBA8). |
| `pc/src/pc_gx_texture.c:301-374` (`vita_defer_tex_delete`, `pc_gx_texture_flush_deferred_deletes`) | Implement 4-frame deferred texture deletion queue to prevent GPU-in-flight use-after-free when recycling textures. |
| `vita/src/vita_platform.c:550-575, 618-626` (`pc_platform_init`, `pc_platform_swap_buffers`) | Configure VitaGL (`vglSetupShaderPatcher`, `vglSetCircularPoolSize`, `vglInitExtended`, `vglUseVram`, triple buffering clear loop, `vglSwapBuffers`) for `glplat*` ABI. |
| `pc/src/pc_disc.c` / `pc/src/pc_dvd.c` / `vita/src/vita_design_fs.c` | Replace GameCube DVD calls with safe path-jailed file I/O under `ux0:data/smstrikers/`. |
| `vita/src/vita_main.c:21-69` (modular startup pipeline) | Implement thin `vita_game_boot`. **Do NOT link full Game `Initialize` / `src/Game/main.cpp` yet** before subsystems pass tests. |
| `vita/src/vita_gx_cmdbuf.c` & `vita/src/vita_gx_compat.c` | Translate display lists to dynamic VBO vertex batches (`glDrawArrays`). No immediate-mode `glBegin`/`glVertex`. |
| `vita/src/vita_frame.c` | Double-buffered frame synchronization and frame pacing. |
| `ATTRIBUTE2=12` extended mem (`vita/CMakeLists.txt:356`) | Add extended memory flag when linking full Game heap. |

**Key discipline:** Do **not** copy ACGC’s emu64/cmdbuf/TEV matrix wholesale — SMS already has the clean NL `gl`/`glx` ABI. Leverage ACGC's proven GXP loading pattern, texture decoders, VitaGL presentation quirks (`vita_gx_compat.c`), and modular boot discipline.

---

## Architecture (fixed decisions)

```
vita/
  CMakeLists.txt          # compiles selected ../src/{Game,NL,ode} + vita/src/plat
  src/main.cpp            # Vita entry → calls Game Initialize path (or thin shim)
  src/plat/
    os/                   # OSAlloc, threads, time → sceKernel*
    dvd/                  # nlFileGC DVD → ux0:data/smstrikers/
    pad/                  # PADRead / VBlankPadUpdate → SceCtrl
    math/                 # platvmath/platqmath soft impl
    gx/                   # glplat* + glx* → VitaGL (VBOs)
    audio/                # PlatAudio stubs → later PCM out
  tools/                  # host: asset extract + endian convert (optional)
  PORT.md                 # this file
```

**Link strategy:** Vita CMake lists portable `.cpp` from `src/Game`, `src/NL`, `src/ode`.  
**Exclude:** `src/Dolphin/**`, `src/PowerPC_EABI_Support/**`, GC-only `src/NL/glx/*.cpp` / `glPlat.cpp` / `nlFileGC.cpp` / `plat/*.cpp` when Vita replacements exist.  
**Provide:** stub/shim `include/dolphin/**` headers under `vita/include/dolphin/` (types only) so NL headers compile.

---

## Phase 1 — P0 Headless link (eboot links)

**What:** Extend `vita/CMakeLists.txt` to compile a minimal slice of `Game`/`NL`/`ode` + stub every unresolved Dolphin/GX symbol. Replace `vita/src/main.cpp` loop with call into a **stubbed** `Initialize` path that logs and returns, OR keep bootstrap and add a second target `smstrikers_vita_linkcheck` that only links.

**Docs to copy:** CMake macros from `vita/CMakeLists.txt`; boot order from `src/Game/main.cpp` / `nlMain.cpp`.

**Implement:**
1. `vita/include/dolphin/` minimal stubs (`os.h`, `dvd.h`, `pad.h`, `mtx.h`, `gx/*` types)
2. `vita/src/plat/os/*` — `OSAlloc`/`OSGetTime`/`OSCreateThread` → malloc / clock / sceKernel
3. Soft `platvmath` / `platqmath` (no `asm { psq_* }`)
4. Stub `glplat*` returning success / no-op draws
5. Stub `nlInitFileSystem` reading from `ux0:` with fopen

**Verify:**
- [x] `cmake --build build/vita` produces `eboot.bin` / VPK
- [x] No edits under `src/` matching paths required for GCN build
- [ ] Device or Vita3K: app starts, logs “P0 link OK”, Start exits *(needs hardware/Vita3K run)*

**Status (2026-09-23):** Done in-tree. Plat stubs under `vita/src/plat/{os,pad,dvd,math,gx}`, dolphin shims in `vita/include/dolphin/`, `p0_boot` exercises alloc/math/glplat/pad.

**Anti-pattern:** Don’t rewrite `configure.py`. Don’t commit assets.

---

## Phase 2 — P1 VFS + input

**What:** Real filesystem + pad so engine can open `common.ini` / early configs and see buttons.

**Docs:** `vita/README.md` data layout; `include/NL/nlFile.h` + `nlFileGC.h` surface; `include/NL/platpad.h`; `vita/src/main.cpp` ctrl pattern; map GC A→Cross, B→Circle, sticks as in platform DIGEST.

**Implement:**
1. `nlOpen`/`nlRead`/`nlSeek` → `ux0:data/smstrikers/...`
2. Document exact extract tree for **G4QE01** (USA) as default supported dump
3. `InitPlatPad` / `VBlankPadUpdate` / `PADRead` shim from `SceCtrlData`
4. PSTV note: L2/R2 via rear touch / DS4 mapping documented in README

**Verify:**
- [x] Green path via `vita_file_open("common.ini")` + clear colors
- [x] A/Cross → blue tint; Start exits via `PAD_BUTTON_START`
- [x] Missing data = amber, still runs
- [ ] Full `nlOpen` ABI wired into Game (still `vita_file_*`)

**Status (2026-09-23):** Pad double-buffer + analog mode; VFS path-jail under `VITA_DATA_ROOT`.

---

## Phase 3 — P2 GXP Simple Blit & Banner Texture Decode

**What:** Replace temporary scissor-clear-only hacks with real textured quad drawing using committed GXP shaders and GameCube texture decoding.

**ACGC reference paths:**
- Shader compiler pipeline: `/tmp/ACGC-Vita-Port/vita/shaders/compile_new_shaders.sh` (Sony `psp2cgc` compiling `.cg` → `.gxp`) and `/tmp/ACGC-Vita-Port/vita/shaders/gen_header.py` (generating `/tmp/ACGC-Vita-Port/vita/include/vita_gxp_shaders.h`).
- Day-to-day compilation: `shacc` / `libshacccg.suprx` is **NOT required** on Vita or Vita3K when precompiled GXP binaries are committed in headers (see SMS [`vita/src/plat/gx/gxp_simple.h`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/src/plat/gx/gxp_simple.h)).
- Direct GXP loading via VitaGL: `/tmp/ACGC-Vita-Port/pc/src/pc_gx_tev.c:680–699` (`vita_load_gxp`), prepending a 4-byte `matrix_uniforms_num = 0` header to raw GXP bytes and calling `glShaderBinary(1, &shader, 0, buf, total)`; retrieved via `vita_get_simple_shader()` (`pc/src/pc_gx_tev.c:158`).
- Banner textured quad drawing: `/tmp/ACGC-Vita-Port/vita/src/vita_banner.c:108–118` (texture creation via `glTexImage2D`) and lines 149–230 (`banner_draw_bars`), streaming quad vertices (`verts[12]`) into a VBO and calling `glDrawArrays(GL_TRIANGLES, 0, 12)` using `vita_get_simple_shader()`. **No scissor-clear-only hacks!**
- GameCube texture decode: `/tmp/ACGC-Vita-Port/pc/src/pc_gx_texture.c:1433–1444` (`decode_RGB5A3`) and lines 1531–1544 (`decode_rgb5a3_entry`). Unpacks GameCube tiled 4×4 RGB5A3 (0x1800 bytes) from `opening.bnr` to 96×32 RGBA8 for VitaGL upload.

**Implement:**
1. Update [`vita/src/plat/gx/bnr_title.cpp`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/src/plat/gx/bnr_title.cpp) to create a VitaGL texture (`glGenTextures`, `glBindTexture`, `glTexImage2D`), stream quad vertices to a VBO, and draw via `glDrawArrays` with [`vita/src/plat/gx/gxp_simple.h`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/src/plat/gx/gxp_simple.h).
2. Delete the per-pixel `glScissor` + `glClear` scanline loop.
3. Validate runtime execution without `libshacccg.suprx` on Vita3K.

**Verify (Vita3K / Hardware checklist):**
- [ ] Title banner textured (real textured quad rendered from `opening.bnr` via `glDrawArrays`, not scissor-clear-only)
- [ ] Start / Select exits cleanly via `PAD_BUTTON_START`
- [ ] `common.ini` green path still valid where applicable (green if `common.ini` present, amber if missing)
- [ ] Zero runtime dependency on `libshacccg.suprx` on Vita3K

**Status (2026-09-23):** Header [`vita/src/plat/gx/gxp_simple.h`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/src/plat/gx/gxp_simple.h) extracted from ACGC and committed in-tree. Ready to replace `bnr_title.cpp` scissor hack.

---

## Phase 4 — P3 `glplat` VitaGL & `nl` Filesystem Layer

**What:** Establish the real platform GL presentation pipeline (`glplat*`) and filesystem bridge (`nlFile*`) to `ux0:data/smstrikers/`, replacing GameCube DVD hardware calls and placeholder frame loops.

**ACGC reference paths:**
- VitaGL platform initialization & present: `/tmp/ACGC-Vita-Port/vita/src/vita_platform.c:550–575` (`pc_platform_init`: `vglSetupShaderPatcher`, `vglSetCircularPoolSize`, `vglInitExtended(256*1024, 960, 544, ...)`, `vglUseVram(GL_TRUE)`, triple backbuffer clear loop).
- Frame buffer swap: `/tmp/ACGC-Vita-Port/vita/src/vita_platform.c:618–626` (`pc_platform_swap_buffers` calling `vglSwapBuffers(GL_FALSE)`).
- Frame pacing & sync: `/tmp/ACGC-Vita-Port/vita/src/vita_frame.c` (double-buffered frame execution and swap timing).
- Filesystem & ROM mounting: `/tmp/ACGC-Vita-Port/pc/src/pc_disc.c` (mounting GC dump / FST table) and `/tmp/ACGC-Vita-Port/pc/src/pc_dvd.c` / `/tmp/ACGC-Vita-Port/vita/src/vita_design_fs.c` (reading assets relative to `ux0:data/<game>/` without Dolphin DVD hardware).

**Implement:**
1. `glplat` VitaGL present path:
   - Implement `include/NL/gl/glPlat.h` and `include/NL/glx/glxSwap.h`:
     * `glplatPreStartup()`, `glplatStartup(gl_ScreenInfo*)`, `glplatPostStartup()`: configure VitaGL (960×544, pool sizes, `vglInitExtended`, `vglUseVram`).
     * `glplatBeginFrame()`, `glplatSendFrame()`, `glplatEndFrame()`, `glplatFinish()`: clear backbuffers, execute queued draw commands, call `vglSwapBuffers(GL_FALSE)`.
2. `nl` FS layer:
   - Implement `include/NL/nlFile.h` and `include/NL/nlFileGC.h`:
     * Bridge `nlOpen`, `nlRead`, `nlSeek`, `nlFileSize`, `nlClose`, `nlInitFileSystem`, `nlLoadEntireFile` to [`vita/src/plat/dvd/fs_vita.cpp`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/src/plat/dvd/fs_vita.cpp) path-jailed under `ux0:data/smstrikers/`.
     * Replace GameCube DVD-specific async/blocking reads (`GameCubeReadBlocking`, `DVDReadAsync`) with native POSIX/SceIo operations.

**Verify:**
- [ ] `glplat` present path: continuous frames at 960×544 presented via `glplatBeginFrame` / `glplatSendFrame` / `glplatEndFrame` / `vglSwapBuffers` without visual tearing or memory leaks
- [ ] `nlFile` ABI reads `common.ini` and extracted dump assets reliably through `nlOpen`/`nlRead`
- [ ] `common.ini` green path remains valid
- [ ] Safe path-jailing strictly rejects directory traversal (`..` or leading `/`)

---

## Phase 5 — P4 Thin Engine Boot (`vita_game_boot`) & Soft Math

**What:** Implement a lightweight, modular engine boot sequence (`vita_game_boot`) to initialize and verify low-level subsystems (Memory, FS, glplat, Pad, Audio stub) in isolation.

**CRITICAL ANTI-PATTERN AVOIDED:**
**Do NOT link full Game `Initialize()` or `src/Game/main.cpp` yet!**
Calling full Game `Initialize()` prematurely pulls in hundreds of unresolved dependencies simultaneously: `CARD` / `gcmemcard` memory cards, serial interface (`SI`), movie playback (`THPSimple`), progressive scan prompts, reset tasks, and the full task manager (`nlTaskManager`). Debugging boot crashes in a monolithic link is nearly impossible.

**ACGC reference paths:**
- Modular startup pipeline: `/tmp/ACGC-Vita-Port/vita/src/vita_main.c:21–69`. ACGC modularizes startup: loads settings (`pc_settings_load`), scans banners (`banner_scan_folder`), initializes platform/GL (`pc_platform_init`), checks disc image (`pc_disc_init`), and only then enters the main game loop.
- Math & NEON SIMD: `/tmp/ACGC-Vita-Port/pc/src/pc_mtx.c:275–277` and `/tmp/ACGC-Vita-Port/vita/src/vita_gx_cmdbuf.c:18` (`<arm_neon.h>`). Replaces PowerPC `psq_*` paired-single assembly with portable C++ / NEON equivalents.

**Implement:**
1. `vita_game_boot`:
   - Create a thin boot runner:
     * `nlInitMemory()` → initialize memory pools and allocators.
     * `nlInitFileSystem()` → mount and verify `ux0:data/smstrikers/`.
     * `glplatPreStartup()` & `glplatStartup()` → establish VitaGL context and display mode.
     * `InitPlatPad()` → initialize controller polling (`SceCtrl`).
     * `vita_audio_init()` → initialize audio placeholder stub.
     * Render textured title banner quad using Phase 3's GXP blit pipeline.
     * Interactive frame loop: verify input (A/Cross tints, Start exits).
     * Clean teardown sequence.
2. Soft math verification:
   - Replace any remaining PPC assembly in `src/NL/plat/platvmath.cpp` and `include/NL/platqmath.h` with portable C++/NEON operations.
3. Endian verification:
   - Maintain decomp's portable `src/NL/nlEndian.cpp` byte swap verification (`nlSwapEndian`).

**Verify (Vita3K / Hardware checklist):**
- [ ] `vita_game_boot` executes all low-level subsystems sequentially and logs clean status for each
- [ ] Title banner textured (real textured quad, not scissor-clear-only) and `common.ini` green path verified during boot
- [ ] Start button cleanly exits loop and terminates process via `PAD_BUTTON_START`
- [ ] Zero PPC `psq_*` assembly or Dolphin DOL symbols required

---

## Phase 6 — P5 FE Textures & GX Batch

**What:** Implement comprehensive GameCube texture format decoders & texture caching for Front-End UI bundles, and implement display list / vertex batching for 2D/UI rendering.

**ACGC reference paths:**
- Texture decoders: `/tmp/ACGC-Vita-Port/pc/src/pc_gx_texture.c:1322–1660` (`decode_gc_texture`):
  * `decode_CMPR` (lines 1479–1520): DXT1 block decoding with 4×4 sub-blocks (used heavily in GameCube UI and 3D textures).
  * `decode_CI4` / `decode_CI8` (lines 1594–1640): Indexed textures with 16-color or 256-color palettes (TLUTs).
  * `decode_I4` / `decode_I8` (lines 1322–1368) and `decode_IA4` / `decode_IA8` (lines 1370–1410): Grayscale & intensity-alpha formats used for fonts and icons.
  * `decode_RGB565` (lines 1411–1432) and `decode_RGBA8` (lines 1452–1477).
- Texture memory safety: `/tmp/ACGC-Vita-Port/pc/src/pc_gx_texture.c:301–374` (`vita_defer_tex_delete` and `pc_gx_texture_flush_deferred_deletes`). 4-frame deferred delete queue preventing GPU-in-flight use-after-free when recycling textures.
- GX Batching & VBO pipeline: `/tmp/ACGC-Vita-Port/vita/src/vita_gx_cmdbuf.c` (double-buffered command queue and vertex batches using `PCGXVertex`), `/tmp/ACGC-Vita-Port/vita/src/vita_gx_compat.c`, and `/tmp/ACGC-Vita-Port/pc/src/pc_gx.c`. Replaces individual immediate-mode calls with vertex buffer streaming (`glBufferData` / `glDrawArrays`).

**Implement:**
1. FE Texture Pipeline:
   - Port GC format decoders (CMPR, CI8, IA8, RGB565, RGBA8) into `vita/src/plat/gx/` supporting `glxTexture.h` formats.
   - Implement texture upload and binding cache for Front-End menus and fonts loaded from `src/NL/nlBundleFile.cpp`.
   - Implement deferred texture deletion queue (aging pipeline) to prevent GPU-in-flight use-after-free during menu transitions.
2. 2D GX Batching:
   - Wire `glxSend` / `glxDisplayList` 2D orthographic quad submission into VitaGL dynamic VBOs.
   - Render Front-End text, menu buttons, and UI frames using simple GXP blit shaders.

**Verify:**
- [ ] UI textures (CMPR, CI8, IA8, RGBA8) from dump bundles decode with proper color channels and alpha transparency
- [ ] 2D UI quads batch into dynamic VBOs and render via `glDrawArrays` with zero immediate-mode calls (`rg 'glBegin' vita/src/` returns 0)
- [ ] Start / Select exits cleanly, navigation works on FE screens

---

## Phase 7 — P6 3D / GX Batch → VitaGL (Match Renderer)

**What:** Full 3D match renderer translating GameCube GX display lists, matrix stack, and multi-stage TEV combiners into VitaGL batches and GXP shaders.

**ACGC reference paths:**
- GX Command buffer & state cache: `/tmp/ACGC-Vita-Port/vita/src/vita_gx_cmdbuf.c`, `/tmp/ACGC-Vita-Port/vita/src/vita_gx_compat.c`, and `/tmp/ACGC-Vita-Port/pc/src/pc_gx.c`.
- Precompiled TEV Shader configs: `/tmp/ACGC-Vita-Port/pc/src/pc_gx_tev.c` (52 specialized shader configs + uber shader, loaded via `vita_load_gxp` with `glShaderBinary`), `/tmp/ACGC-Vita-Port/vita/shaders/compile_new_shaders.sh`, and `/tmp/ACGC-Vita-Port/vita/shaders/gen_header.py`.
- Compositing and FBO: `/tmp/ACGC-Vita-Port/vita/shaders/composite_v.cg`, `composite_f.cg` for depth-aware effects.

**Implement order:**
1. Matrix stack: `glplatSetMatrix` / modelview / projection transform pipeline.
2. Display list → VBO batch packer: convert GX display lists (`src/NL/glx/glxDisplayList.*`, `glxSend.cpp`) into interleaved vertex buffers.
3. TEV combiner translation: map `glxGX.cpp` TEV stages to specialized GXP shader permutations or multi-stage shaders.
4. Render targets & EFB copy: implement framebuffer copies for stadium and ball effects.
5. LOD / crowd impostors if perf tanks.

**Verify:**
- [ ] In-match field + players recognizable at 30fps target on device
- [ ] Frame-time log; drop effects before dropping input

---

## Phase 8 — P7 Audio

**What:** `PlatAudio` implementation without GameCube DSP.

**Docs & ACGC reference:** `include/NL/plat/plataudio.h`; SDK `psp2/audioout.h` + `SceAudio_stub` (prefer over NGS); `/tmp/ACGC-Vita-Port/pc/src/pc_audio.c` and `/tmp/ACGC-Vita-Port/vita/src/vita_platform.c:581`.

**Implement:**
1. Stub all `PlatAudio::*` to silence (ship FE mute).
2. Software mixer or subset of MusyX if sources allow; else decode assets to PCM.
3. Dedicated audio output thread → `sceAudioOutOpenPort` / `Output` / `ReleasePort`.

**Verify:**
- [ ] SFX/music on match start; no underrun spam
- [ ] Mute path still works if audio init fails

---

## Phase 9 — P8 Playable vertical slice + polish

**What:** One exhibition match end-to-end: boot → FE → kickoff → goal → pause → quit.

**Checklist:**
- [ ] Saves (replace `gcmemcard` with file under `ux0:data/smstrikers/saves/`)
- [ ] PSTV control profile
- [ ] Performance pass (AI tick budget, crowd)
- [ ] Separate CI: `.github/workflows/vita.yml` with VitaSDK Docker (does not replace GCN CI)
- [ ] README: legal dump steps, precompiled GXP shaders, known limitations

---

## Phase 10 — Verification gate (full port)

| Check | How |
|-------|-----|
| GCN decomp untouched | `git diff` excludes matching-critical paths; GCN CI green |
| No assets in git | `git status` / CI grep for `.thp`/`.glt`/ISO |
| ABI coverage | Script: every `glplat*` / pad / file symbol either Vita impl or listed stub |
| Playable | Device checklist Phase 9 |
| Docs | `vita/README.md` updated from this plan’s user-facing bits |

---

## Suggested execution order for agents

1. Phase 1 (link) — **first code** (done in-tree)
2. Phase 2 (VFS+pad) (done in-tree)
3. Phase 3 (GXP blit + tex decode) — replace scissor-clear hack with real textured quad using committed GXP
4. Phase 4 (`glplat` VitaGL + `nl` FS layer) — real frame present loop + `nlFile` disk reads
5. Phase 5 (thin engine boot: `vita_game_boot`) — verify low-level subsystems without full monolithic Game Initialize
6. Phase 6 (FE textures + GX batch) — UI bundle texture decoders and dynamic VBO batching
7. Phase 7 (3D GX batch & TEV shaders) — match renderer
8. Phase 8 (audio PCM)
9. Phase 9 (playable exhibition match vertical slice)
10. Phase 10 (verification gate)

Trigger implementation with `/do` or “execute Phase N” against this file.

---

## Open questions (resolve before Phase 7)

1. ~~Supported dump version~~ → **G4QE01** (decided)
2. ~~Audio strategy~~ → **SceAudioOut PCM thread** first; MusyX software / preconverted assets only if needed later
3. Vita3K-first vs hardware-first for daily iteration?
4. Keep title ID `VSTR00001` or register a proper one later?
