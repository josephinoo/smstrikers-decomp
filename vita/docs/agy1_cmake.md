# Unit 1 Digest: Align SMS `vita/CMakeLists.txt` with ACGC Vita Patterns

## Overview

Unit 1 aligns the Super Mario Strikers (SMS) Vita CMake build configuration with the patterns established in [ACGC-Vita-Port](https://github.com/Brendonm17/ACGC-Vita-Port) while strictly maintaining the existing bootstrap functionality and constraints.

## Changes Made to `vita/CMakeLists.txt`

1. **Toolchain Fallback Configuration**:
   - Added automatic detection of `CMAKE_TOOLCHAIN_FILE` from `$ENV{VITASDK}/share/vita.toolchain.cmake` if not explicitly specified on the command line.

2. **Project Metadata Variables**:
   - Extracted `VITA_APP_NAME` ("SM Strikers Vita"), `VITA_TITLEID` ("VSTR00001"), and `VITA_VERSION` ("00.07").
   - Bumped `VITA_VERSION` from `00.06` to `00.07`.

3. **Compilation Definitions**:
   - Added global `add_compile_definitions(TARGET_VITA __vita__)` matching the ACGC platform targeting pattern.

4. **Preserved Bootstrap Components**:
   - Maintained `src/plat/gx/vgl_splash_stub.c` (required to override VitaGL's background splash thread which crashes Vita3K).
   - Maintained `src/plat/gx/bnr_title.cpp` (banner title decoding and presentation).
   - Preserved all existing platform stub sources (`os_vita.cpp`, `pad_vita.cpp`, `fs_vita.cpp`, `vmath_vita.cpp`, `glplat_stub.cpp`, `audio_stub.cpp`, `p0_boot.cpp`, `nlEndian.cpp`).

5. **Documented Link Set**:
   - Added grouped documentation comments categorizing all linked libraries:
     * VitaGL & shader compiler stubs (`vitaGL`, `vitashark`, `SceShaccCgExt`, `taihen_stub`, `SceShaccCg_stub`)
     * Hardware acceleration & DMA (`mathneon`, `SceKernelDmacMgr_stub`)
     * Vita OS, graphics & input services (`SceCtrl_stub`, `SceDisplay_stub`, `SceGxm_stub`, `SceCommonDialog_stub`, `SceSysmodule_stub`, `SceAppMgr_stub`, `SceAppUtil_stub`)
     * Kernel & system memory (`SceLibKernel_stub`, `SceKernelThreadMgr_stub`, `SceSysmem_stub`)
     * Standard runtime (`m`, `stdc++`)
   - Documented rationale for omitted libraries (e.g. `SceAudio_stub` deferred to Phase 8 per `PORT.md`; SDL2 not needed).

6. **Extended Memory Configuration**:
   - Added `set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")` before `vita_create_vpk` to enable extended memory (109MB extra RAM budget on retail Vita / PSTV).

7. **VPK Asset Packaging**:
   - Replaced `${CMAKE_SOURCE_DIR}` references with `${CMAKE_CURRENT_SOURCE_DIR}` for robust out-of-tree / subproject inclusion.
   - Verified `<source> <dest>` argument ordering for `vita_create_vpk`.
   - Added explicit note that custom GXP and runtime shader packaging (`shaders/*.vert`, `shaders/*.frag` or compiled GXP headers) are deferred to later phases, keeping the VPK strictly asset-free for now.

## Differences Still Present Between SMS and ACGC

| Area | ACGC Vita Port | SMS Vita Port (Unit 1) | Rationale |
|------|----------------|------------------------|-----------|
| **Decomp Sources** | Compiles full game C/C++ decomp tree into static `game_data` lib and code objects | Minimal platform layer (`src/plat/*`) + `nlEndian.cpp` | SMS is progressing through bootstrap phases; game logic linkage lands in subsequent phases. |
| **Compiler Flags** | Complex UB mitigation (`-Os -g1`, `-Og`, `-fno-tree-vrp`, `-fwrapv`, `-flto`, NEON flags) | Standard C++17 with `-Wall -Wextra -Wpedantic` | Bootstrap layer is modern C++ without legacy UB compiler issues. Decomp optimizations will be tuned when integrating matching sources. |
| **Linker Symbols** | `-Wl,--undefined=_newlib_heap_size_user`, `-Wl,--undefined=sceUserMainThreadStackSize`, `-flto`, `--gc-sections` | Default VitaSDK linker options | Stack and newlib heap symbols not yet customized in SMS bootstrap. |
| **Dependencies** | Links `SDL2`, `SceAudio_stub`, `SceAudioIn_stub`, `SceTouch_stub`, `SceNpTrophy_stub`, `SceHid_stub`, `SceMotion_stub`, `pthread`, `z`, `c` | Links only VitaGL, mathneon, and base Vita OS stubs | Minimal dependencies; audio is stubbed until Phase 8, input uses native `SceCtrl_stub` without SDL2 overhead. |
| **Shader Packaging** | Ships `default.vert` and `default.frag` in VPK + embeds 52 precompiled GXP shaders in `vita_gxp_shaders.h` | No shaders in VPK; bootstrap uses VitaGL default pipeline and solid-color/banner blits | Shader packaging and TEV combiners arrive in later rendering units. |
| **LiveArea Paths** | Assets located in `vita/sce_sys/` | Assets located in `vita/livearea/sce_sys/` | Avoids file churn in repo while maintaining identical destination layout in VPK (`sce_sys/`). |
| **Splash Screen** | Default VitaGL splash / black screen | Explicit stub (`vgl_splash_stub.c`) | Avoids VitaGL splash thread crashes on Vita3K. |

## Verification

- Configured and built cleanly with `arm-vita-eabi-gcc` 15.2.0 and `vita.cmake`.
- Verified `param.sfo` generated with `APP_VER=00.07`, `TITLE_ID=VSTR00001`, and `ATTRIBUTE2=12`.
- Verified `smstrikers-vita.vpk` packages `eboot.bin`, `param.sfo`, and LiveArea icons with zero game assets or external shaders.
