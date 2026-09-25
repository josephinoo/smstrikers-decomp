# Unit 7/10: Thin Game Boot Slice

## Overview

Unit 7 implements a thin, controlled game boot slice (`vita/src/plat/vita_game_boot.cpp`) for the Super Mario Strikers PS Vita port. Rather than attempting a full link of `src/Game/main.cpp` (which pulls in PPC-specific assembly, complete task graph systems, unported frontend managers, and GC hardware drivers), this unit isolates the fundamental initialization sequence into an observable, non-hanging state machine.

---

## 1. Boot Chain Architecture & Sequence

### 1.1 Comparison with Retail GameCube `Initialize()` (`src/Game/main.cpp`)

| Step | Retail GameCube (`src/Game/main.cpp`) | Unit 7 Thin Boot Slice (`vita_game_boot.cpp`) |
| :--- | :--- | :--- |
| **1. Filesystem / Data** | Register DVD error & reset callbacks via `nlRegHandleDVD*` | Probes `ux0:data/smstrikers/` mount via `p0_mount_data()` / `vita_data_present()` |
| **2. Engine Base** | `nlInit()` | Subsumed by platform runtime initialization |
| **3. GL Subsystem** | `glStartup()` (initializes views, states, matrices, font, rasterizers) | Stubs `glStartup()` returning `true` (`glplatStartup` / VitaGL context already active) |
| **4. Global Textures** | `glLoadTextureBundle("global.glt")` (decompresses and uploads global bundle) | Stubs `glLoadTextureBundle("global.glt")` returning `true`; probes VFS for asset presence |
| **5. Config Dictionaries** | `Config::Global().LoadFromFile("common.ini")`, `platform.ini`, `locale.ini`, `user.ini` | Weak NL config hook (`nl_config_load_file`); falls back to VFS probing of all 4 INI files |
| **6. Subsystems** | `EventManager`, `nlTaskManager`, `AudioLoader`, `CARDInit`, `PADSetSamplingCallback` | Intentionally excluded from thin boot link; kept out to prevent unresolved symbol cascades |

### 1.2 State Machine Stages

`vita_game_boot` transitions through the following discrete stages, logging state progression to stdout:

1. `VITA_BOOT_STAGE_INIT`: Initializes reporting structures and diagnostic headers.
2. `VITA_BOOT_STAGE_MOUNT_DATA`: Verifies accessibility of `ux0:data/smstrikers` (checks `common.ini`). Supports amber-path operation if data has not yet been staged.
3. `VITA_BOOT_STAGE_LOAD_CONFIG`: Checks for weak symbol `nl_config_load_file`. If present, calls it for `common.ini`, `platform.ini`, `locale.ini`, and `user.ini`. If absent, inspects each file via the platform VFS (`vita_file_open` / `vita_file_size`).
4. `VITA_BOOT_STAGE_GL_STARTUP`: Dispatches `glStartup()`. Verifies positive return.
5. `VITA_BOOT_STAGE_GLOBAL_TEXTURES`: Dispatches `glLoadTextureBundle("global.glt")`. Stubs `true` to allow boot progression even when assets are missing or BE-packed.
6. `VITA_BOOT_STAGE_COMPLETE`: Concludes the slice, returning `VitaGameBootStatus` (`VITA_BOOT_OK` or `VITA_BOOT_DATA_MISSING`).

---

## 2. Key Decisions

### 2.1 Invocation Point: On Start in Title Screen Loop
- **Decision**: `vita_game_boot()` is invoked when the user presses **Start** (or Z trigger) in `vita/src/main.cpp`, rather than running immediately before the main frame loop.
- **Rationale**:
  - **Console Fidelity**: In the retail game, attract mode / title art is shown first. User input (Start) triggers the transition out of the title screen into game modes.
  - **Interactivity & Visual Verification**: Immediate execution before the title loop would risk hanging or delaying the presentation of `opening.bnr` art and controller responsiveness (such as the Cross button blue background tint).
  - **Non-hanging Execution**: When Start is pressed, the boot state machine runs, prints the complete step-by-step diagnostic log to stdout, logs the return enum, and exits cleanly (`main` returns 0). This prevents deadlocks on both physical Vita hardware and Vita3K.

### 2.2 Exclusion of `src/Game/main.cpp` from CMake
- `src/Game/main.cpp` contains references to `AudioLoader`, `EventManager`, `ResetTask`, `LoadingManager`, `ReplayManager`, `MemCard`, and Metrowerks runtime facilities. Adding it directly to CMake at this phase would require hundreds of unverified stubs across unrelated subsystems.
- Unit 7 implements the thin boot slice independently in `vita/src/plat/vita_game_boot.cpp`.

### 2.3 Stubbing Policy for CARD, SI, and Audio
- No calls to `CARDInit()`, `SISetSamplingRate()`, or audio streaming were referenced in `vita_game_boot.cpp`.
- Consequently, no dummy CARD or SI stubs were added to the link, maintaining minimal diff footprint and zero dead linker dependencies.

### 2.4 C++ Linkage for Engine Stubs
- Functions `bool glStartup(void)` and `bool glLoadTextureBundle(const char* filename)` are defined with standard C++ global linkage, matching `include/NL/gl/gl.h`.
- The public platform API (`vita_game_boot`, `vita_game_boot_status_str`, `VitaGameBootStatus`) maintains C ABI linkage (`extern "C"`) in `vita/include/plat_abi.h`.

---

## 3. Files Changed / Created

- `vita/src/plat/vita_game_boot.cpp` (created): Thin game boot slice implementation and state machine.
- `vita/include/plat_abi.h` (modified): Declared `VitaGameBootStatus` enum, `vita_game_boot()`, `vita_game_boot_status_str()`, and engine GL stubs.
- `vita/src/main.cpp` (modified): Wired Start button press to call `vita_game_boot()` and print the diagnostic result before exiting.
- `vita/CMakeLists.txt` (modified): Added `src/plat/vita_game_boot.cpp` to `smstrikers_vita` sources.
- `vita/docs/agy7_game_boot.md` (created): This documentation digest.

---

## 4. Verification

- **Compilation**: Built with VitaSDK `arm-vita-eabi-g++` (C++17, `-Wall -Wextra -Wpedantic`). Zero errors, zero warnings.
- **Package Generation**: Successfully generated `eboot.bin` and `smstrikers-vita.vpk`.
- **Symbol Inspection**:
  - `vita_game_boot` (Text symbol, C linkage)
  - `vita_game_boot_status_str` (Text symbol, C linkage)
  - `_Z9glStartupv` (Text symbol, C++ `glStartup()`)
  - `_Z19glLoadTextureBundlePKc` (Text symbol, C++ `glLoadTextureBundle(const char*)`)
