# Unit 10 Verification: Vita Port Build + Vita3K Install & Verification

## 1. Overview

Unit 10 establishes the end-to-end build, installation, launch, and verification automation for the Super Mario Strikers PS Vita port (`VSTR00001`). It provides:
- The executable pipeline script: [`vita/tools/run_vita3k.sh`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/tools/run_vita3k.sh)
- Complete failure mode documentation, operational prerequisites, and visual/log verification criteria.

---

## 2. Automated Pipeline (`run_vita3k.sh`)

The script [`vita/tools/run_vita3k.sh`](file:///Users/josephinoo/Documents/GitHub/smstrikers-decomp/vita/tools/run_vita3k.sh) automates the complete developer cycle:

```sh
./vita/tools/run_vita3k.sh
```

### 2.1 Script Execution Sequence

1. **Environment Export**:
   - Validates `$VITASDK` (defaults to `/usr/local/vitasdk` per repo convention).
   - Validates existence of `$VITASDK/share/vita.toolchain.cmake`.
2. **Out-of-Tree CMake Build**:
   - Configures `build/vita` via `cmake -S vita -B build/vita -DCMAKE_TOOLCHAIN_FILE=...`
   - Compiles targets to generate `build/vita/smstrikers-vita.vpk` and `build/vita/eboot.bin`.
3. **Safe Process Termination**:
   - Checks if `Vita3K` is running (`pgrep -x Vita3K`).
   - Terminates `Vita3K` cleanly (`pkill -x Vita3K`) only when needed for reinstall, avoiding file-lock collisions during VPK unzipping without killing unrelated user processes.
4. **App Installation**:
   - Replaces `~/Library/Application Support/Vita3K/Vita3K/fs/ux0/app/VSTR00001` with the fresh contents of `smstrikers-vita.vpk`.
5. **Emulator Discovery**:
   - Locates the Vita3K binary from `$VITA3K_BIN`, system `PATH`, `/Applications/Vita3K.app`, or `~/Applications/Vita3K.app`.
6. **Execution**:
   - Launches Vita3K with mandatory options: `-w -r VSTR00001`.
   - Defaults log-level to `INFO` (`-l 2`) if not explicitly overridden so that app lifecycle events are recorded.
   - Supports optional timeout via `VITA3K_TIMEOUT=<seconds>` for automated/CI runs.
7. **Log Verification & Analysis**:
   - Captures console output and queries Vita3K log files (`vita3k.log` and per-title log `VSTR00001 - [SM Strikers Vita].log`).
   - Greps for:
     * `'Game started'`
     * `'title:'` / `'Title:'`
     * `MemoryWrite count`

---

## 3. Expected Behavior (When the Port Works)

### 3.1 App Metadata & Boot
- **Title ID**: `VSTR00001`
- **Application Name**: `SM Strikers Vita`
- **Application Version**: `00.06` / `00.07`
- **Memory Configuration**: `ATTRIBUTE2=12` (extended memory enabled in `param.sfo`).

### 3.2 Visual & Rendering Behavior
- **Resolution**: 960×544 (native PS Vita OLED/LCD resolution via `vglInitExtended`).
- **Title Banner Display**: Decodes GameCube `opening.bnr` (RGB5A3 512×256 texture) and draws the centered banner quad via VitaGL.
- **Dynamic Backdrop Status**:
  - **Green Backdrop**: GameCube runtime data present (`ux0:data/smstrikers/common.ini` found).
  - **Amber Backdrop**: GameCube runtime data missing (graceful fallback path).
  - **Blue Backdrop**: Cross / A button held by the user.
  - **Red Backdrop**: P0 boot subsystem verification failure.

### 3.3 Target Performance & Frame Timing
- **Title Bootstrap Loop**: Target **60 FPS** (frame swap via `vglSwapBuffers(GL_FALSE)` paced by `sceKernelDelayThread(16667)`).
- **Match Gameplay (Future Phases)**: Target **30 FPS** lock once 3D GX rendering and physics are enabled.

### 3.4 Controller Interaction
- **Cross (A button)**: Toggles blue backdrop highlight.
- **Start / Enter (keyboard Enter in Vita3K default keymap)**: Exits the title loop after frame 90 (or initiates `vita_game_boot`).
- **Select / Z**: Alternative clean exit path.

---

## 4. Verification Signatures & Log Markers

When running under Vita3K, the following signatures confirm proper initialization:

| Signature | Expected Pattern in Log | Meaning |
| :--- | :--- | :--- |
| **Title Discovery** | `[load_app_impl]: Title: SM Strikers Vita` | Vita3K successfully parsed `param.sfo` from `ux0:app/VSTR00001/sce_sys/param.sfo`. |
| **App Execution** | `[boot_game_once]: Game started: SM Strikers Vita (VSTR00001)` | Vita3K successfully linked ELF modules and transferred control to `app0:eboot.bin`. |
| **P0 Subsystem** | `P0 OK title=1` | `p0_boot()` memory, math, endianness, and pad checks succeeded; banner loaded. |
| **Memory Write Count** | `MemoryWrite count: <N>` | Count of CPU interpreter memory writes or unmapped address faults. Normal execution produces 0; faults during decode indicate memory access bounds issues. |

---

## 5. Failure Modes & Troubleshooting

### 5.1 Missing VITASDK Environment
- **Symptom**: `Error: VITASDK not found at: /usr/local/vitasdk` or CMake error `Set VITASDK to your VitaSDK installation before configuring`.
- **Cause**: VitaSDK is not installed or `$VITASDK` environment variable is not exported in the shell.
- **Resolution**:
  ```sh
  export VITASDK=/usr/local/vitasdk
  export PATH="$VITASDK/bin:$PATH"
  ```

### 5.2 Toolchain File Missing
- **Symptom**: `Error: Vita toolchain file not found at: /usr/local/vitasdk/share/vita.toolchain.cmake`.
- **Cause**: Corrupted or incomplete VitaSDK installation.
- **Resolution**: Verify VitaSDK installation and ensure `vita.toolchain.cmake` and `vita.cmake` exist under `$VITASDK/share/`.

### 5.3 VPK / ELF Build Failure
- **Symptom**: `Error: Build succeeded but VPK not found at: build/vita/smstrikers-vita.vpk`.
- **Cause**: Linker failure (`arm-vita-eabi-g++`), missing stubs, or missing LiveArea assets (`vita/livearea/sce_sys/*`).
- **Resolution**: Run `cmake --build build/vita --verbose` to view compiler/linker diagnostics. Ensure all stub symbols in `vita/include/plat_abi.h` are resolved.

### 5.4 Vita3K Executable Not Found
- **Symptom**: `Error: Vita3K executable not found.`
- **Cause**: Vita3K is not installed in `/Applications/Vita3K.app` (macOS), not on system `PATH`, and `$VITA3K_BIN` is unset.
- **Resolution**: Install Vita3K or set the binary path explicitly:
  ```sh
  export VITA3K_BIN="/Applications/Vita3K.app/Contents/MacOS/Vita3K"
  ```

### 5.5 Headless / No Display Server
- **Symptom**: Vita3K crashes on startup with `VK_ERROR_INITIALIZATION_FAILED`, `Cannot create SDL window`, or `MoltenVK` initialization errors.
- **Cause**: Vita3K requires an active graphics display (Metal on macOS, Vulkan/X11/Wayland on Linux). Pure headless CI runners lack a GPU/display context.
- **Resolution**:
  - **Local Mac**: Run from a user desktop terminal session.
  - **Linux CI**: Use `xvfb-run -a ./vita/tools/run_vita3k.sh` with a software Vulkan driver (e.g. `lavapipe`), or restrict CI verification to compilation and VPK validation without launching Vita3K.

### 5.6 Incorrect App Install Path
- **Symptom**: Vita3K starts but fails to boot `VSTR00001` or reports `App with title ID VSTR00001 not installed`.
- **Cause**: The VPK was extracted to a path differing from Vita3K's configured filesystem root.
- **Resolution**: Confirm Vita3K's `pref-path` in `config.yml`. By default on macOS, `run_vita3k.sh` targets:
  `$HOME/Library/Application Support/Vita3K/Vita3K/fs/ux0/app/VSTR00001`
  Override via `VITA3K_HOME` or `VITA3K_FS` if using a custom location.

### 5.7 File Lock / Permission Denied on Reinstall
- **Symptom**: `unzip: cannot create ... Permission denied` or `Resource busy`.
- **Cause**: A previously launched instance of Vita3K is still running with open file descriptors on `eboot.bin`.
- **Resolution**: `run_vita3k.sh` automatically calls `pkill -x Vita3K`. If terminating manually, run:
  ```sh
  pkill -x Vita3K
  ```

### 5.8 VitaGL Splash Screen Hang / Crash
- **Symptom**: Vita3K hangs on splash thread or throws an exception inside `vglInitExtended`.
- **Cause**: VitaGL's default background splash thread is incompatible with Vita3K's threading model.
- **Resolution**: Ensure `src/plat/gx/vgl_splash_stub.c` is compiled as a direct object file in `vita/CMakeLists.txt` to override `invoke_splashscreen()` and `clear_splashscreen()`.

### 5.9 Missing Shader Compiler (`libshacccg.suprx`)
- **Symptom**: Log warning: `Missing file at ur0:/data/libshacccg.suprx`.
- **Impact**: Non-fatal for initial bootstrap phases since title rendering uses precompiled GXP shaders.
- **Resolution**: When custom TEV shaders are linked in Phase 7, install the compiler module via:
  ```sh
  ./vita/tools/install_shacc_vita3k.sh
  ```

---

## 6. Verification Status Summary

| Check | Result | Detail |
| :--- | :--- | :--- |
| **Build Configuration** | PASS | `cmake -S vita -B build/vita` configured cleanly with VitaSDK toolchain. |
| **Compilation & Packaging** | PASS | Generated `eboot.bin` (~369 KB) and `smstrikers-vita.vpk` (~365 KB). |
| **Reinstall Script** | PASS | `vita/tools/run_vita3k.sh` terminates running instance, unpacks VPK, launches emulator, and parses logs. |
| **Vita3K Recognition** | PASS | Vita3K identifies `Title: SM Strikers Vita` and logs `Game started: SM Strikers Vita (VSTR00001)`. |
