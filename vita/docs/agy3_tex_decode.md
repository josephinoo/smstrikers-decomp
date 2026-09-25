# Unit 3 Digest: Port GameCube Texture Decoders to SMS Vita Island

## Files Changed

- `vita/include/plat/gc_tex_decode.h` (created):
  * C++ and C compatible header defining prototypes for expanding GameCube tiled texture formats into row-major linear RGBA8 buffers.
  * Formats covered: RGB5A3, RGB565, RGBA8, CMPR (DXT1-like), CI8, CI4, I4, I8, IA4, IA8.
  * TLUT / palette builders for RGB5A3, RGB565, and IA8 with grayscale ramp fallback.
  * Unified `gc_decode_texture` dispatcher matching GX format codes.
  * Attribution comments citing ACGC-PC-Port / ACGC-Vita-Port (`pc_gx_texture.c`, MIT License).

- `vita/src/plat/gx/gc_tex_decode.cpp` (created):
  * Self-contained decoder implementations adapted to C++17 with `size_t` dimensions and pointer offsets.
  * Big-endian pixel reading, tile indexing, CMPR sub-block interpolation, and two-pass RGBA8 unpacking.
  * Zero external dependencies beyond standard library (`<cstddef>`, `<cstdint>`, `<cstring>`).

- `vita/tools/check_gc_tex_decode.cpp` (created):
  * Host-runnable self-check (tagged `ponytail`) asserting known golden bytes for hand-crafted RGB5A3, CMPR, CI8, and RGBA8 tiles.
  * Runnable with standard host compilers (`clang++` or `g++`) without needing VitaSDK.

- `vita/CMakeLists.txt` (modified):
  * Added `src/plat/gx/gc_tex_decode.cpp` to the `smstrikers_vita` executable sources list.

- `vita/docs/agy3_tex_decode.md` (created):
  * This documentation digest.

## Key Decisions

1. **Tiling Notes**:
   - GameCube textures are stored in 2D hardware tiles rather than linear scanlines. Tile dimensions vary by format:
     * 4x4 tiles (16 pixels): RGB5A3 (16bpp, 32B), RGB565 (16bpp, 32B), IA8 (16bpp, 32B), RGBA8 (32bpp, 64B).
     * 8x4 tiles (32 pixels): I8 (8bpp, 32B), IA4 (8bpp, 32B), CI8 (8bpp indexed, 32B).
     * 8x8 tiles (64 pixels): I4 (4bpp, 32B), CI4 (4bpp indexed, 32B).
     * 8x8 super-blocks: CMPR (4 sub-blocks of 4x4 DXT1, 32B).
   - In memory, texture data is padded to complete tile boundaries. Loop counters advance the source pointer through full tile byte counts (`bw = (w + block_w - 1) / block_w`, `bh = (h + block_h - 1) / block_h`), while destination writing clips cleanly via `if (px < w && py < h)`.
   - RGBA8 stores each 4x4 tile across two 32-byte passes: the first pass stores 16 `[Alpha, Red]` pairs, and the second pass stores 16 `[Green, Blue]` pairs.

2. **CMPR Endianness & Layout**:
   - Each 8x8 super-block contains four 4x4 DXT1 sub-blocks organized in Z-order:
     * Sub 0: Top-Left `(x: 0..3, y: 0..3)`
     * Sub 1: Top-Right `(x: 4..7, y: 0..3)`
     * Sub 2: Bottom-Left `(x: 0..3, y: 4..7)`
     * Sub 3: Bottom-Right `(x: 4..7, y: 4..7)`
   - The two 16-bit reference colors `c0` and `c1` are stored in big-endian byte order (`(src[0] << 8) | src[1]`).
   - When `c0 > c1`, 4 opaque colors are generated (2/3 and 1/3 linear blends). When `c0 <= c1`, 3 colors are generated (1/2 blend) and color index 3 represents fully transparent black `(0, 0, 0, 0)`.

3. **Palette (TLUT) Lookup**:
   - GameCube indexed textures (CI8, CI4) use 16-bit big-endian palette entries formatted as RGB5A3, RGB565, or IA8.
   - `gc_build_palette` expands up to 256 entries into an RGBA8 palette array. If palette data is absent or uninitialized, it populates a linear grayscale ramp `(i, i, i, 255)` so textures remain visually debuggable rather than corrupting memory.
   - `gc_decode_ci8_rgb5a3` provides a single-step decode combining palette expansion and indexed tile decode.

4. **Self-Contained & C++ Friendly**:
   - All buffer sizes and coordinates use `size_t` to ensure clean 64-bit host tool execution and 32-bit ARM Vita execution.
   - No Dolphin/GX headers or PC emulator headers are included; format constants and signatures are declared independently in `plat/gc_tex_decode.h`.

## How to Run the Self-Check

The test compiles and runs on any host machine using standard `clang++` or `g++`:

```bash
# Compile and run host self-check
clang++ -std=c++17 -Wall -Wextra -Wpedantic -I vita/include \
    vita/src/plat/gx/gc_tex_decode.cpp vita/tools/check_gc_tex_decode.cpp \
    -o vita/tools/check_gc_tex_decode && ./vita/tools/check_gc_tex_decode

# Clean up binary
rm -f vita/tools/check_gc_tex_decode
```

Expected output:
```
check_gc_tex_decode: ALL PASS (ponytail)
```

## Context for Next Step

With self-contained GameCube texture decoders in place, the port can convert GameCube texture assets (both raw disc `.glt` / bundle files and runtime banner art) into linear RGBA8 buffers ready for VitaGL texture upload via `glTexImage2D`. In Phase 6 (P5 2D / UI via VitaGL), these routines will back the frontend texture loader, font renderer, and replace the preliminary ad-hoc RGB5A3 decoder in `bnr_title.cpp`.
