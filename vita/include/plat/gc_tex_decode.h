// gc_tex_decode.h - GameCube texture format decoders
//
// Ported from ACGC-PC-Port / ACGC-Vita-Port (pc_gx_texture.c)
// Sources:
//   https://github.com/flyngmt/ACGC-PC-Port (MIT License)
//   https://github.com/Brendonm17/ACGC-Vita-Port (MIT License)
//
// Decodes GameCube tiled textures (RGB5A3, RGB565, RGBA8, CMPR, CI8, I4, I8, etc.)
// into row-major linear RGBA8 buffers.

#ifndef PLAT_GC_TEX_DECODE_H
#define PLAT_GC_TEX_DECODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GameCube texture format constants (matching GXTexFmt / GXCITexFmt)
enum GCTextureFormat {
    GC_TF_I4     = 0x0,
    GC_TF_I8     = 0x1,
    GC_TF_IA4    = 0x2,
    GC_TF_IA8    = 0x3,
    GC_TF_RGB565 = 0x4,
    GC_TF_RGB5A3 = 0x5,
    GC_TF_RGBA8  = 0x6,
    GC_TF_C4     = 0x8,
    GC_TF_C8     = 0x9,
    GC_TF_CMPR   = 0xE,
};

// GameCube TLUT (palette) format constants (matching GXTlutFmt)
enum GCTlutFormat {
    GC_TL_IA8    = 0x0,
    GC_TL_RGB565 = 0x1,
    GC_TL_RGB5A3 = 0x2,
};

// Pixel-level decoders
void gc_decode_rgb5a3_pixel(uint16_t val, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
void gc_decode_rgb565_pixel(uint16_t val, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

// Palette builders (converts raw TLUT data into a 256-entry RGBA8 palette)
void gc_build_palette(const void* tlut_data, int tlut_fmt, size_t n_entries,
                      uint8_t palette[256][4], bool is_be);
void gc_build_palette_rgb5a3(const void* tlut_data, size_t n_entries,
                             uint8_t palette[256][4], bool is_be);
void gc_build_palette_rgb565(const void* tlut_data, size_t n_entries,
                             uint8_t palette[256][4], bool is_be);
void gc_build_palette_ia8(const void* tlut_data, size_t n_entries,
                          uint8_t palette[256][4], bool is_be);

// Individual tiled format decoders (expanding to row-major linear RGBA8 dst buffer of w * h * 4 bytes)
void gc_decode_i4(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_i8(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_ia4(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_ia8(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_rgb565(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_rgb5a3(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_rgba8(const void* src, uint8_t* dst, size_t w, size_t h);
void gc_decode_cmpr(const void* src, uint8_t* dst, size_t w, size_t h);

// Indexed formats
void gc_decode_ci4(const void* src, uint8_t* dst, size_t w, size_t h, const uint8_t palette[256][4]);
void gc_decode_ci8(const void* src, uint8_t* dst, size_t w, size_t h, const uint8_t palette[256][4]);

// Combined helper for CI8 with RGB5A3 TLUT
void gc_decode_ci8_rgb5a3(const void* src, uint8_t* dst, size_t w, size_t h,
                          const void* tlut_data, size_t tlut_entries, bool is_be);

// Unified texture decoder dispatcher matching GX format codes
bool gc_decode_texture(const void* src, uint8_t* dst, size_t w, size_t h,
                       uint32_t format, const uint8_t palette[256][4]);

#ifdef __cplusplus
}
#endif

#endif // PLAT_GC_TEX_DECODE_H
