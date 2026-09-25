// gc_tex_decode.cpp - GameCube texture format decoders
//
// Ported from ACGC-PC-Port / ACGC-Vita-Port (pc_gx_texture.c)
// Sources:
//   https://github.com/flyngmt/ACGC-PC-Port (MIT License)
//   https://github.com/Brendonm17/ACGC-Vita-Port (MIT License)
//
// Decodes GameCube tiled textures (RGB5A3, RGB565, RGBA8, CMPR, CI8, I4, I8, etc.)
// into row-major linear RGBA8 buffers.

#include "plat/gc_tex_decode.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

inline uint16_t read_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}

} // namespace

extern "C" {

void gc_decode_rgb5a3_pixel(uint16_t val, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if ((val & 0x8000u) != 0) {
        // RGB555 opaque
        *r = static_cast<uint8_t>(((val >> 10) & 0x1Fu) * 255 / 31);
        *g = static_cast<uint8_t>(((val >> 5) & 0x1Fu) * 255 / 31);
        *b = static_cast<uint8_t>((val & 0x1Fu) * 255 / 31);
        *a = 255;
    } else {
        // ARGB3444
        *a = static_cast<uint8_t>(((val >> 12) & 0x07u) * 255 / 7);
        *r = static_cast<uint8_t>(((val >> 8) & 0x0Fu) * 255 / 15);
        *g = static_cast<uint8_t>(((val >> 4) & 0x0Fu) * 255 / 15);
        *b = static_cast<uint8_t>((val & 0x0Fu) * 255 / 15);
    }
}

void gc_decode_rgb565_pixel(uint16_t val, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    *r = static_cast<uint8_t>(((val >> 11) & 0x1Fu) * 255 / 31);
    *g = static_cast<uint8_t>(((val >> 5) & 0x3Fu) * 255 / 63);
    *b = static_cast<uint8_t>((val & 0x1Fu) * 255 / 31);
    *a = 255;
}

void gc_build_palette(const void* tlut_data, int tlut_fmt, size_t n_entries,
                      uint8_t palette[256][4], bool is_be) {
    if (!palette) {
        return;
    }

    // Default fallback: grayscale ramp
    for (size_t i = 0; i < 256; ++i) {
        palette[i][0] = static_cast<uint8_t>(i);
        palette[i][1] = static_cast<uint8_t>(i);
        palette[i][2] = static_cast<uint8_t>(i);
        palette[i][3] = 255;
    }

    if (!tlut_data || n_entries == 0) {
        return;
    }
    if (n_entries > 256) {
        n_entries = 256;
    }

    const uint8_t* pal_bytes = static_cast<const uint8_t*>(tlut_data);
    const uint16_t* pal16 = static_cast<const uint16_t*>(tlut_data);

    for (size_t i = 0; i < n_entries; ++i) {
        uint16_t val;
        if (is_be) {
            val = read_be16(pal_bytes + i * 2);
        } else {
            val = pal16[i];
        }

        if (tlut_fmt == GC_TL_RGB5A3) {
            gc_decode_rgb5a3_pixel(val, &palette[i][0], &palette[i][1],
                                   &palette[i][2], &palette[i][3]);
        } else if (tlut_fmt == GC_TL_RGB565) {
            gc_decode_rgb565_pixel(val, &palette[i][0], &palette[i][1],
                                   &palette[i][2], &palette[i][3]);
        } else {
            // GC_TL_IA8: high byte = intensity, low byte = alpha
            if (is_be) {
                palette[i][0] = static_cast<uint8_t>(val >> 8);
                palette[i][1] = static_cast<uint8_t>(val >> 8);
                palette[i][2] = static_cast<uint8_t>(val >> 8);
                palette[i][3] = static_cast<uint8_t>(val & 0xFF);
            } else {
                palette[i][0] = static_cast<uint8_t>(val & 0xFF);
                palette[i][1] = static_cast<uint8_t>(val & 0xFF);
                palette[i][2] = static_cast<uint8_t>(val & 0xFF);
                palette[i][3] = static_cast<uint8_t>(val >> 8);
            }
        }
    }
}

void gc_build_palette_rgb5a3(const void* tlut_data, size_t n_entries,
                             uint8_t palette[256][4], bool is_be) {
    gc_build_palette(tlut_data, GC_TL_RGB5A3, n_entries, palette, is_be);
}

void gc_build_palette_rgb565(const void* tlut_data, size_t n_entries,
                             uint8_t palette[256][4], bool is_be) {
    gc_build_palette(tlut_data, GC_TL_RGB565, n_entries, palette, is_be);
}

void gc_build_palette_ia8(const void* tlut_data, size_t n_entries,
                          uint8_t palette[256][4], bool is_be) {
    gc_build_palette(tlut_data, GC_TL_IA8, n_entries, palette, is_be);
}

// I4: 8x8 blocks, 4bpp, 2 pixels per byte
void gc_decode_i4(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 7) / 8;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 8; ++y) {
                for (size_t x = 0; x < 8; x += 2) {
                    const uint8_t val = *src++;
                    const size_t px0 = bx * 8 + x;
                    const size_t py = by * 8 + y;
                    const size_t px1 = px0 + 1;
                    const uint8_t i0 = static_cast<uint8_t>((val >> 4) | (val & 0xF0));
                    const uint8_t i1 = static_cast<uint8_t>((val & 0x0F) | ((val & 0x0F) << 4));
                    if (px0 < w && py < h) {
                        const size_t idx = (py * w + px0) * 4;
                        dst[idx + 0] = i0;
                        dst[idx + 1] = i0;
                        dst[idx + 2] = i0;
                        dst[idx + 3] = i0;
                    }
                    if (px1 < w && py < h) {
                        const size_t idx = (py * w + px1) * 4;
                        dst[idx + 0] = i1;
                        dst[idx + 1] = i1;
                        dst[idx + 2] = i1;
                        dst[idx + 3] = i1;
                    }
                }
            }
        }
    }
}

// I8: 8x4 blocks, 8bpp
void gc_decode_i8(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 8; ++x) {
                    const uint8_t val = *src++;
                    const size_t px = bx * 8 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const size_t idx = (py * w + px) * 4;
                        dst[idx + 0] = val;
                        dst[idx + 1] = val;
                        dst[idx + 2] = val;
                        dst[idx + 3] = val;
                    }
                }
            }
        }
    }
}

// IA4: 8x4 blocks, 8bpp (high nibble = alpha, low nibble = intensity)
void gc_decode_ia4(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 8; ++x) {
                    const uint8_t val = *src++;
                    const size_t px = bx * 8 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const uint8_t a = static_cast<uint8_t>((val >> 4) | (val & 0xF0));
                        const uint8_t i = static_cast<uint8_t>((val & 0x0F) | ((val & 0x0F) << 4));
                        const size_t idx = (py * w + px) * 4;
                        dst[idx + 0] = i;
                        dst[idx + 1] = i;
                        dst[idx + 2] = i;
                        dst[idx + 3] = a;
                    }
                }
            }
        }
    }
}

// IA8: 4x4 blocks, 16bpp (alpha byte + intensity byte)
void gc_decode_ia8(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 3) / 4;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 4; ++x) {
                    const uint8_t a = *src++;
                    const uint8_t i = *src++;
                    const size_t px = bx * 4 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const size_t idx = (py * w + px) * 4;
                        dst[idx + 0] = i;
                        dst[idx + 1] = i;
                        dst[idx + 2] = i;
                        dst[idx + 3] = a;
                    }
                }
            }
        }
    }
}

// RGB565: 4x4 blocks, 16bpp
void gc_decode_rgb565(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 3) / 4;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 4; ++x) {
                    const uint16_t val = read_be16(src);
                    src += 2;
                    const size_t px = bx * 4 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const size_t idx = (py * w + px) * 4;
                        gc_decode_rgb565_pixel(val, &dst[idx + 0], &dst[idx + 1],
                                               &dst[idx + 2], &dst[idx + 3]);
                    }
                }
            }
        }
    }
}

// RGB5A3: 4x4 blocks, 16bpp (Bit 15=1: RGB555 opaque, Bit 15=0: ARGB3444)
void gc_decode_rgb5a3(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 3) / 4;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 4; ++x) {
                    const uint16_t val = read_be16(src);
                    src += 2;
                    const size_t px = bx * 4 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const size_t idx = (py * w + px) * 4;
                        gc_decode_rgb5a3_pixel(val, &dst[idx + 0], &dst[idx + 1],
                                               &dst[idx + 2], &dst[idx + 3]);
                    }
                }
            }
        }
    }
}

// RGBA8: 4x4 blocks, 32bpp (two passes: AR then GB, 64 bytes total per block)
void gc_decode_rgba8(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 3) / 4;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            uint8_t ar[16][2]; // AR pass (32 bytes)
            for (size_t i = 0; i < 16; ++i) {
                ar[i][0] = *src++; // Alpha
                ar[i][1] = *src++; // Red
            }
            for (size_t i = 0; i < 16; ++i) { // GB pass (32 bytes)
                const size_t x = i % 4;
                const size_t y = i / 4;
                const size_t px = bx * 4 + x;
                const size_t py = by * 4 + y;
                const uint8_t g = *src++;
                const uint8_t b = *src++;
                if (px < w && py < h) {
                    const size_t idx = (py * w + px) * 4;
                    dst[idx + 0] = ar[i][1]; // R
                    dst[idx + 1] = g;        // G
                    dst[idx + 2] = b;        // B
                    dst[idx + 3] = ar[i][0]; // A
                }
            }
        }
    }
}

// CMPR (DXT1): 8x8 super-blocks of 2x2 sub-blocks (each sub-block is 4x4 DXT1)
void gc_decode_cmpr(const void* src_ptr, uint8_t* dst, size_t w, size_t h) {
    if (!src_ptr || !dst || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 7) / 8;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (int sub = 0; sub < 4; ++sub) {
                const size_t sx = static_cast<size_t>((sub & 1) * 4);
                const size_t sy = static_cast<size_t>((sub >> 1) * 4);
                const uint16_t c0 = read_be16(src);
                const uint16_t c1 = read_be16(src + 2);
                src += 4;

                uint8_t palette[4][4];
                palette[0][0] = static_cast<uint8_t>(((c0 >> 11) & 0x1Fu) * 255 / 31);
                palette[0][1] = static_cast<uint8_t>(((c0 >> 5) & 0x3Fu) * 255 / 63);
                palette[0][2] = static_cast<uint8_t>((c0 & 0x1Fu) * 255 / 31);
                palette[0][3] = 255;

                palette[1][0] = static_cast<uint8_t>(((c1 >> 11) & 0x1Fu) * 255 / 31);
                palette[1][1] = static_cast<uint8_t>(((c1 >> 5) & 0x3Fu) * 255 / 63);
                palette[1][2] = static_cast<uint8_t>((c1 & 0x1Fu) * 255 / 31);
                palette[1][3] = 255;

                // Interpolated colors
                if (c0 > c1) {
                    for (int c = 0; c < 3; ++c) {
                        palette[2][c] = static_cast<uint8_t>((2 * palette[0][c] + palette[1][c]) / 3);
                        palette[3][c] = static_cast<uint8_t>((palette[0][c] + 2 * palette[1][c]) / 3);
                    }
                    palette[2][3] = 255;
                    palette[3][3] = 255;
                } else {
                    for (int c = 0; c < 3; ++c) {
                        palette[2][c] = static_cast<uint8_t>((palette[0][c] + palette[1][c]) / 2);
                    }
                    palette[2][3] = 255;
                    palette[3][0] = 0;
                    palette[3][1] = 0;
                    palette[3][2] = 0;
                    palette[3][3] = 0;
                }

                for (size_t y = 0; y < 4; ++y) {
                    const uint8_t row = *src++;
                    for (size_t x = 0; x < 4; ++x) {
                        const int ci = (row >> (6 - x * 2)) & 3;
                        const size_t px = bx * 8 + sx + x;
                        const size_t py = by * 8 + sy + y;
                        if (px < w && py < h) {
                            const size_t idx = (py * w + px) * 4;
                            dst[idx + 0] = palette[ci][0];
                            dst[idx + 1] = palette[ci][1];
                            dst[idx + 2] = palette[ci][2];
                            dst[idx + 3] = palette[ci][3];
                        }
                    }
                }
            }
        }
    }
}

// CI4: 8x8 blocks, 4bpp indexed
void gc_decode_ci4(const void* src_ptr, uint8_t* dst, size_t w, size_t h,
                   const uint8_t palette[256][4]) {
    if (!src_ptr || !dst || !palette || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 7) / 8;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 8; ++y) {
                for (size_t x = 0; x < 8; x += 2) {
                    const uint8_t val = *src++;
                    const size_t px0 = bx * 8 + x;
                    const size_t py = by * 8 + y;
                    const size_t px1 = px0 + 1;
                    const uint8_t ci0 = (val >> 4) & 0x0Fu;
                    const uint8_t ci1 = val & 0x0Fu;
                    if (px0 < w && py < h) {
                        const size_t idx = (py * w + px0) * 4;
                        dst[idx + 0] = palette[ci0][0];
                        dst[idx + 1] = palette[ci0][1];
                        dst[idx + 2] = palette[ci0][2];
                        dst[idx + 3] = palette[ci0][3];
                    }
                    if (px1 < w && py < h) {
                        const size_t idx = (py * w + px1) * 4;
                        dst[idx + 0] = palette[ci1][0];
                        dst[idx + 1] = palette[ci1][1];
                        dst[idx + 2] = palette[ci1][2];
                        dst[idx + 3] = palette[ci1][3];
                    }
                }
            }
        }
    }
}

// CI8: 8x4 blocks, 8bpp indexed
void gc_decode_ci8(const void* src_ptr, uint8_t* dst, size_t w, size_t h,
                   const uint8_t palette[256][4]) {
    if (!src_ptr || !dst || !palette || w == 0 || h == 0) return;
    const uint8_t* src = static_cast<const uint8_t*>(src_ptr);
    const size_t bw = (w + 7) / 8;
    const size_t bh = (h + 3) / 4;
    for (size_t by = 0; by < bh; ++by) {
        for (size_t bx = 0; bx < bw; ++bx) {
            for (size_t y = 0; y < 4; ++y) {
                for (size_t x = 0; x < 8; ++x) {
                    const uint8_t val = *src++;
                    const size_t px = bx * 8 + x;
                    const size_t py = by * 4 + y;
                    if (px < w && py < h) {
                        const size_t idx = (py * w + px) * 4;
                        dst[idx + 0] = palette[val][0];
                        dst[idx + 1] = palette[val][1];
                        dst[idx + 2] = palette[val][2];
                        dst[idx + 3] = palette[val][3];
                    }
                }
            }
        }
    }
}

void gc_decode_ci8_rgb5a3(const void* src, uint8_t* dst, size_t w, size_t h,
                          const void* tlut_data, size_t tlut_entries, bool is_be) {
    uint8_t palette[256][4];
    gc_build_palette_rgb5a3(tlut_data, tlut_entries, palette, is_be);
    gc_decode_ci8(src, dst, w, h, palette);
}

bool gc_decode_texture(const void* src, uint8_t* dst, size_t w, size_t h,
                       uint32_t format, const uint8_t palette[256][4]) {
    if (!src || !dst || w == 0 || h == 0) return false;

    switch (format) {
        case GC_TF_I4:
            gc_decode_i4(src, dst, w, h);
            return true;
        case GC_TF_I8:
            gc_decode_i8(src, dst, w, h);
            return true;
        case GC_TF_IA4:
            gc_decode_ia4(src, dst, w, h);
            return true;
        case GC_TF_IA8:
            gc_decode_ia8(src, dst, w, h);
            return true;
        case GC_TF_RGB565:
            gc_decode_rgb565(src, dst, w, h);
            return true;
        case GC_TF_RGB5A3:
            gc_decode_rgb5a3(src, dst, w, h);
            return true;
        case GC_TF_RGBA8:
            gc_decode_rgba8(src, dst, w, h);
            return true;
        case GC_TF_C4:
            if (!palette) return false;
            gc_decode_ci4(src, dst, w, h, palette);
            return true;
        case GC_TF_C8:
            if (!palette) return false;
            gc_decode_ci8(src, dst, w, h, palette);
            return true;
        case GC_TF_CMPR:
            gc_decode_cmpr(src, dst, w, h);
            return true;
        default:
            std::memset(dst, 0, w * h * 4);
            return false;
    }
}

} // extern "C"
