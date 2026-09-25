# SM Strikers Vita bootstrap

This directory is the independent PS Vita/PS TV build target. It does not
compile the GameCube executable and the resulting VPK contains no original
game data.

## Build

```sh
export VITASDK=/usr/local/vitasdk
cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build/vita
```

The output is `build/vita/smstrikers-vita.vpk`. Install it with VitaShell / Vita3K.

**Vita3K notes:**
- Bare `sceDisplay` runs but does **not** present — emulator stays on
  “Please wait, loading…”. Bootstrap presents with **vitaGL**.
- VitaGL’s splash thread crashes Vita3K; we stub those symbols at link time
  (`src/plat/gx/vgl_splash_stub.c`, same approach as vitaForge).

## Vita3K (PC)

1. Rebuild + reinstall `smstrikers-vita.vpk`.
2. Shows textured **Super Mario Strikers** banner from dump `opening.bnr` (rendered as a textured quad via GXP simple blit, not scissor-clear-only hacks).
3. `common.ini` green/amber path remains valid (`common.ini` present = green, missing = amber).
4. Cross → blue backdrop. **Start** / **Select** exits.
5. Full playable match is still later phases (FE → GX → audio). This is real game art on the boot path.

**Reference port & shaders:**
- ACGC Vita reference: clone with `git clone -b vita https://github.com/Brendonm17/ACGC-Vita-Port.git` (use branch `vita`; `master` is PC).
- `libshacccg.suprx` / runtime shacc is **not** required for day-to-day compilation or running on Vita3K/Vita because GXP shaders are committed as precompiled C headers (`vita_gxp_shaders.h` / `gxp_simple.h`), exactly following the ACGC pattern. Keep ShaRKBR33D staged (`./vita/tools/install_shacc_vita3k.sh`) only if rebuilding raw runtime shaders.
- Guide (for manual extraction if ever needed): https://cimmerian.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx

## Data layout

Extract files from a legally owned copy of the supported GameCube release and
place the port's runtime data under:

```
ux0:data/smstrikers/
```

The bootstrap checks for `ux0:data/smstrikers/common.ini` and verifies runtime subsystems:
- **Green**: data present (`common.ini` found)
- **Amber**: no data (`common.ini` missing, link OK)
- **Blue**: A (Cross) button pressed / held
- **Red**: boot fail (`p0_boot` failed subsystem checks)
- Press **Start** to exit.

Phase 3 adds the platform audio stub (`vita_audio_init` / `vita_audio_shutdown`, silent placeholder before Phase 8 `sceAudioOutOpenPort`) and endian check (`nlSwapEndian` 0x1234 → 0x3412 verification using decomp's portable `src/NL/nlEndian.cpp`).

The next porting steps belong behind this boundary: file loading, controller
mapping, saves/audio, and the GX-to-VitaGL renderer replacement. Do not put
disc images or extracted game assets in this repository or VPK.

Full phased plan (ABI, phases P0–P8, verify gates): see [PORT.md](PORT.md).
