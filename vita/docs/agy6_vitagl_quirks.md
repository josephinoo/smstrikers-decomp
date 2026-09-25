# VitaGL Quirks, Workarounds, and Compatibility Guide (Unit 6/10)

This document provides an actionable analysis of VitaGL quirks and workarounds discovered in **[ACGC-Vita-Port](https://github.com/Brendonm17/ACGC-Vita-Port)** (`vita_gx_compat.c`, `vita_platform.c`, `vita_uniform.h`) and outlines concrete implementation steps for the **Super Mario Strikers (SMS)** PS Vita port.

---

## 1. ACGC `vita_gx_compat.c` Workarounds Matrix

| Symptom | ACGC Fix | SMS Action |
| :--- | :--- | :--- |
| **`glDrawBuffers` unsupported in VitaGL**<br>Calling `glDrawBuffers` causes link error or crash because VitaGL lacks Multiple Render Target (MRT) support. | Implemented no-op stub in `vita_gx_compat.c`: `void vita_glDrawBuffers(int n, const unsigned int* bufs) { (void)n; (void)bufs; }`. | **Safe to stub.** GameCube GX uses a single Embedded Framebuffer (EFB) color buffer target at any given time. Add one-liner no-op stub `vita_glDrawBuffers` to `vita/src/plat/gx/vgl_compat.cpp` so any ported GL pipeline code calling MRT functions links cleanly. |
| **`glReadBuffer` unsupported in VitaGL**<br>VitaGL does not implement `glReadBuffer` for selecting framebuffer color buffers for reading. | Implemented no-op stub in `vita_gx_compat.c`: `void vita_glReadBuffer(unsigned int mode) { (void)mode; }`. | **Safe to stub.** GX framebuffer reads (e.g. snapshots, EFB copies) on GameCube are dispatched via EFB copy commands (`GXCopyTex`, `GXCopyDisp`), not GL read buffers. Add no-op stub `vita_glReadBuffer` to `vita/src/plat/gx/vgl_compat.cpp`. |
| **`glUniform*` crash on location `-1`**<br>In standard desktop OpenGL / GLES, calling `glUniform*` with location `-1` is explicitly specified as a silent no-op (used when uniforms are absent or optimized out by the compiler). In VitaGL, `glUniform*` indexes an internal uniform array or dereferences pointers without checking for negative locations, resulting in memory corruption or crash. | Implemented `vita_safe_uniform*` family (1i, 1f, 2f, 3f, 4f, 2fv, 3fv, 4fv, Matrix3fv, Matrix4fv) guarding each call with `if (location >= 0)`. | **Implement safe one-liners.** Add `vita_safe_uniform*` functions to `vita/src/plat/gx/vgl_compat.cpp`. Ensure that all SMS GX emulation and shader uniform setters in Phase 6/7 either call `vita_safe_uniform*` or guard calls with `if (location >= 0)`. |
| **`glClear()` clobbers active shader program**<br>In VitaGL, calling `glClear()` binds an internal clear shader/pipeline, resetting or clobbering the currently active GL shader program. Subsequent draw calls render with improper shader state or fail. | Saved active program in static variable via `vita_save_program(unsigned int program)` and restored it after clear calls via `vita_restore_program_after_clear()` (`if (vita_last_program != 0) glUseProgram(vita_last_program);`). | **Implement program tracking.** Provide `vita_save_program` and `vita_restore_program_after_clear` in `vgl_compat.cpp`. In `glplatBeginFrame` and any SMS draw routine that issues `glClear()`, restore the active shader program immediately after clearing. |
| **PC texture pack DDS loader incompatible with Vita RAM**<br>Desktop PC ports use heavy uncompressed DDS/ZIP texture loaders, which exhaust Vita RAM (512MB total system RAM) and cause severe CPU decompression stutter. | Replaced PC DDS loader with Vita Texture Cache (VTC, `vita_vtc_*`), supplying pre-baked DXT1/DXT5 textures directly to SGX543 GPU decompression hardware. Stubbed out PC async texture streaming functions (`pc_texture_pack_start_async`, `pc_texture_pack_queue_async`, etc.). | **Use native GC formats + VRAM.** GameCube textures already use native CMPR (identical to S3TC DXT1, supported directly by Vita GPU via `GL_COMPRESSED_RGB_S3TC_DXT1_EXT`), RGB5A3, CI8, and RGBA8. Decode GC textures directly into VitaGL textures in Phase 6/7; stub out PC-style async pack APIs if encountered. |
| **Texture cache LRU eviction corrupts bound texture IDs**<br>When a texture cache LRU evicts a texture while an active display list or engine cache still holds a reference, VitaGL recycles the GL texture ID, causing the sampler to display incorrect texture content. | `pc_texture_pack_lookup` returns an `out_key` (`unsigned long long* out_key`) so the caller's cache retains a reference and explicitly releases loaded cache entries upon eviction. | **Track texture handle lifecycles.** In SMS texture caching (`glplatTexture*` / `include/NL/glx/glxTexture.h`), ensure GL texture IDs are never recycled while referenced in pending render lists. Assign unique keys or increment generation counters on eviction. |
| **PC model viewer / debug harness unresolved symbols**<br>PC debug dispatch tables reference PC-specific model viewer functions (`pc_model_viewer_init`, `pc_model_viewer_cleanup`), pulling in heavy desktop SDL/windowing dependencies. | Implemented empty stubs: `void pc_model_viewer_init(GAME* game) { (void)game; }` and `void pc_model_viewer_cleanup(GAME* game) { (void)game; }`. | **Keep debug tools stubbed.** Ensure all GameCube retail debug tools, THP movie players, and PC model viewer routines in `src/Game` are safely stubbed or guarded behind `#ifndef TARGET_VITA`. |

---

## 2. Platform & Runtime Quirks (from ACGC `vita_platform.c` & `vita_uniform.h`)

| Symptom | ACGC Fix | SMS Action |
| :--- | :--- | :--- |
| **VitaGL boot splashscreen thread hangs Vita3K**<br>Standard VitaGL (`libvitaGL.a`) includes `splashscreen.o`, which spawns a dedicated background thread (`"vitaGL Splashscreen"`) to render an animated 3D splash. On Vita3K emulator and in certain sync paths, this thread deadlocks on semaphores (`splash_mutex`) or crashes before main render begins. | ACGC did not add code stubs in-repo (built against custom VitaGL with `NO_SPLASHSCREEN=1` or ran on physical Vita where GXM semas execute without emulator lockup). | **KEEP SMS SPLASH STUBS.** SMS must keep `vita/src/plat/gx/vgl_splash_stub.c` linked directly as an object file. It satisfies linker symbols (`is_splashscreen_active = 0`, `splash_mutex[2] = {0,0}`, `invoke_splashscreen()`, `clear_splashscreen()`, `vglGetCaveBuffer()`), preventing `splashscreen.o` from being pulled from `libvitaGL.a`. (See Section 3 for full confirmation). |
| **Multi-threaded VitaGL call crash**<br>VitaGL and its underlying GXM context are strictly single-threaded. Calling GL functions from secondary worker threads causes race conditions and GPU driver crashes. | Worker thread (`emu64_worker_func`) sets `vita_on_worker_thread = 1` and skips all GL calls. Geometry commands are buffered to a command buffer (`vita_cmdbuf`) and executed exclusively on the main thread (Core 0). | **Single-thread all VitaGL access.** Ensure all `gl*` and `vgl*` calls execute strictly on the main thread (Core 0). Audio, file I/O, and physics tasks in SMS must never touch VitaGL. |
| **Visual garbage in backbuffers on startup**<br>At boot, the triple backbuffer chain in Vita VRAM contains uninitialized memory, leading to visual static or flashing junk frames during early load. | Clears all 3 backbuffers immediately after `vglInitExtended()`: `for (int i = 0; i < 3; i++) { glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT \| GL_DEPTH_BUFFER_BIT); vglSwapBuffers(GL_FALSE); }`. | **Flush 3 backbuffers on startup.** Include 3x black clear and buffer swap in `glplatStartup()` or platform initialization once swap chain is active. |
| **Shader patcher and circular buffer exhaustion**<br>Default VitaGL memory pools are insufficient for complex scenes, resulting in shader compilation failures or vertex buffer overruns. | Configured generous pools before initialization:<br>`vglSetupShaderPatcher(4 * 1024 * 1024, 2 * 1024 * 1024, 2 * 1024 * 1024);`<br>`vglSetCircularPoolSize(32 * 1024 * 1024);`<br>`vglInitExtended(256 * 1024, 960, 544, 32 * 1024 * 1024, msaa_mode);`<br>`vglUseVram(GL_TRUE);`. | **Adopt ACGC pool sizing in Phase 6/7.** SMS matches require rendering stadium models, characters, crowd, and ball physics. Sizing shader patcher (4MB/2MB/2MB) and circular pool (32MB) avoids runtime GPU memory panics. |
| **Stack overflow in deep call chains / heap overcommit**<br>Default 1MB main thread stack overflows on deep Game/NL recursion. Over-allocating newlib heap exhausts physical RAM needed by VitaGL for textures and VRAM. | Explicitly sets:<br>`unsigned int _newlib_heap_size_user = 104 * 1024 * 1024;`<br>`unsigned int sceUserMainThreadStackSize = 4 * 1024 * 1024;`. | **Adopt stack and heap limits.** Keep main thread stack at 4MB in `os_vita.cpp` / platform entry to prevent stack corruption. Size newlib heap appropriately so VitaGL CDRAM/VRAM allocations succeed. |
| **Thread contention across CPU cores**<br>Allowing worker, audio, and main threads to float across cores causes CPU cache thrashing and audio buffer underruns. | Explicitly pins threads:<br>Core 0: Main thread (`SCE_KERNEL_CPU_MASK_USER_0`) & SDL Audio callback.<br>Core 1: Worker / Emu thread (`SCE_KERNEL_CPU_MASK_USER_1`).<br>Core 2: VTC I/O, audio producer, GC (`SCE_KERNEL_CPU_MASK_USER_2`). | **Pin SMS threads.** Pin main loop / rendering to Core 0, pad polling/game tasks to Core 0/1, and background audio streaming to Core 2. |
| **Power / suspend events missed**<br>PS Vita power button and PS button suspend do not trigger standard POSIX signals or SDL quit events, risking unsaved data and desynchronized clocks. | Registers kernel power callback for `SCE_POWER_CB_APP_SUSPEND \| SCE_POWER_CB_BUTTON_PS_PRESS \| SCE_POWER_CB_BUTTON_POWER_PRESS \| SCE_POWER_CB_BUTTON_POWER_HOLD` on dedicated helper thread. Uses `time(NULL)` delta (`>= 2s`) in polling loop to detect resume. | **Implement power callback in OS layer.** Register power callback in `vita/src/plat/os/os_vita.cpp` to pause game timers and flush state when device enters standby. |

---

## 3. Splashscreen Stub Confirmation & Vita3K Compatibility

### 3.1 ACGC vs SMS Behavior
- **ACGC**: ACGC does **not** contain code-level splash stubs in its source tree. The ACGC project was targeted and tested on physical PS Vita hardware and utilized a custom VitaGL branch (`Brendonm17/vitaGL` `@async-compressed-tex-prep`) compiled with `-DNO_SPLASHSCREEN=1`.
- **SMS Port**: The SMS port targets both **Vita3K** (for rapid emulator-based iteration) and **physical PS Vita** hardware.

### 3.2 Why the Splashscreen Hangs Vita3K
In standard `libvitaGL.a`:
1. `_vglInitExtended()` calls `invoke_splashscreen()`.
2. `invoke_splashscreen()` creates a kernel thread (`"vitaGL Splashscreen"`) and allocates two semaphores:
   - `splash_mutex[0]` ("vitaGL Splashscreen Sema Push")
   - `splash_mutex[1]` ("vitaGL Splashscreen Sema Pull")
3. The splash thread attempts to decompress embedded LZ77 3D models and synchronously coordinate with the GXM display queue via `splash_mutex`.
4. On Vita3K (and during early unthrottled boot), semaphore signaling between the main thread and splash thread often deadlocks, or shader compilation fails before the first frame is displayed.

### 3.3 Confirmation of Current SMS Splash Stub
The SMS repository already contains `vita/src/plat/gx/vgl_splash_stub.c`:
```c
#include <stddef.h>
#include <stdint.h>

// Match splashscreen.c: never set active — gxm.c skips sema dance when 0.
uint8_t is_splashscreen_active = 0;
int32_t splash_mutex[2] = {0, 0};

void invoke_splashscreen(void) {}
void clear_splashscreen(void) {}

void* vglGetCaveBuffer(size_t* sz)
{
    if (sz != NULL) {
        *sz = 0;
    }
    return NULL;
}
```

And in `vita/CMakeLists.txt`:
```cmake
# Splash stub .c must be a regular object (not in an archive) so the linker
# resolves vitaGL's splash imports from us and never pulls splashscreen.o.
add_executable(smstrikers_vita
    ...
    src/plat/gx/vgl_splash_stub.c
    ...
)
```

**Status**:
- **Confirmed correct**: Because `vgl_splash_stub.c` is compiled as a direct object in the `smstrikers_vita` executable, the GNU linker resolves all splash-related symbols from `vgl_splash_stub.c`.
- `libvitaGL.a`'s internal `splashscreen.o` is **never pulled in**.
- `is_splashscreen_active` remains `0`, so `gxm.c`'s `is_splashscreen_active` checks skip all semaphore waits.
- **DO NOT REMOVE OR MODIFY** this file. It is essential for Vita3K compatibility and achieves the exact intent of ACGC's `NO_SPLASHSCREEN` build without requiring a custom VitaSDK/VitaGL package.

---

## 4. Immediate Code Additions (`vgl_compat.cpp`)

Following the directive to keep diffs minimal and add only safe, non-breaking one-liners matching ACGC, `vita/src/plat/gx/vgl_compat.cpp` provides:
1. `vita_glDrawBuffers`: safe no-op stub for MRT calls.
2. `vita_glReadBuffer`: safe no-op stub for buffer read selection.
3. `vita_safe_uniform*` family: location >= 0 guards preventing VitaGL crashes.
4. `vita_save_program` / `vita_restore_program_after_clear`: program state preservation across `glClear`.

All implementations are strictly non-breaking one-liners with standard C linkage.
