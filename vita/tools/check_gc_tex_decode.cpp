// check_gc_tex_decode.cpp - Tiny host-runnable self-check for GC texture decoders
//
// ponytail: tiny self-check asserting known golden bytes for hand-crafted GameCube tiles
//
// Build and run on host:
//   clang++ -std=c++17 -Wall -Wextra -I vita/include \
//       vita/src/plat/gx/gc_tex_decode.cpp vita/tools/check_gc_tex_decode.cpp \
//       -o vita/tools/check_gc_tex_decode && ./vita/tools/check_gc_tex_decode

#include "plat/gc_tex_decode.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void check_pixel(const uint8_t* p, uint8_t r, uint8_t g, uint8_t b, uint8_t a, const char* name, int x, int y) {
    if (p[0] != r || p[1] != g || p[2] != b || p[3] != a) {
        std::fprintf(stderr, "FAIL: %s at (%d,%d): expected (%u,%u,%u,%u), got (%u,%u,%u,%u)\n",
                     name, x, y, r, g, b, a, p[0], p[1], p[2], p[3]);
        std::exit(1);
    }
}

static void test_rgb5a3_tile(void) {
    // 4x4 tile = 16 pixels * 2 bytes = 32 bytes (BE)
    // Row 0:
    //   (0,0): 0xFFFF -> RGB555 white (255, 255, 255, 255)
    //   (1,0): 0x8000 -> RGB555 black (0, 0, 0, 255)
    //   (2,0): 0xFC00 -> RGB555 red (255, 0, 0, 255)
    //   (3,0): 0x83E0 -> RGB555 green (0, 255, 0, 255)
    // Row 1:
    //   (0,1): 0x801F -> RGB555 blue (0, 0, 255, 255)
    //   (1,1): 0x0F00 -> ARGB3444 transparent red (255, 0, 0, 0)
    //   (2,1): 0x70F0 -> ARGB3444 opaque green (0, 255, 0, 255)
    //   (3,1): 0x700F -> ARGB3444 opaque blue (0, 0, 255, 255)
    const uint8_t raw_tile[32] = {
        // Row 0
        0xFF, 0xFF,  0x80, 0x00,  0xFC, 0x00,  0x83, 0xE0,
        // Row 1
        0x80, 0x1F,  0x0F, 0x00,  0x70, 0xF0,  0x70, 0x0F,
        // Rows 2 & 3 (padding zeros)
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };

    uint8_t dst[4 * 4 * 4];
    std::memset(dst, 0xCC, sizeof(dst));

    gc_decode_rgb5a3(raw_tile, dst, 4, 4);

    check_pixel(&dst[(0 * 4 + 0) * 4], 255, 255, 255, 255, "RGB5A3", 0, 0);
    check_pixel(&dst[(0 * 4 + 1) * 4], 0, 0, 0, 255, "RGB5A3", 1, 0);
    check_pixel(&dst[(0 * 4 + 2) * 4], 255, 0, 0, 255, "RGB5A3", 2, 0);
    check_pixel(&dst[(0 * 4 + 3) * 4], 0, 255, 0, 255, "RGB5A3", 3, 0);

    check_pixel(&dst[(1 * 4 + 0) * 4], 0, 0, 255, 255, "RGB5A3", 0, 1);
    check_pixel(&dst[(1 * 4 + 1) * 4], 255, 0, 0, 0, "RGB5A3", 1, 1);
    check_pixel(&dst[(1 * 4 + 2) * 4], 0, 255, 0, 255, "RGB5A3", 2, 1);
    check_pixel(&dst[(1 * 4 + 3) * 4], 0, 0, 255, 255, "RGB5A3", 3, 1);
}

static void test_cmpr_block(void) {
    // 8x8 block = 4 sub-blocks of 8 bytes = 32 bytes total.
    // Sub-block 0 (sx=0, sy=0):
    //   c0 = 0xF800 (Red in RGB565)
    //   c1 = 0x001F (Blue in RGB565)
    //   c0 > c1 -> 4-color mode:
    //     p0 = (255, 0, 0, 255)
    //     p1 = (0, 0, 255, 255)
    //     p2 = (170, 0, 85, 255)
    //     p3 = (85, 0, 170, 255)
    //   row 0 = 0x00 (ci=0 -> p0)
    //   row 1 = 0x55 (ci=1 -> p1)
    //   row 2 = 0xAA (ci=2 -> p2)
    //   row 3 = 0xFF (ci=3 -> p3)
    // Sub-block 1 (sx=4, sy=0):
    //   c0 = 0x001F, c1 = 0xF800 (c0 < c1 -> 3-color + transparent mode)
    //   row 0 = 0xFF (ci=3 -> transparent black (0,0,0,0))
    // Sub-blocks 2 & 3: zeros
    uint8_t raw_cmpr[32] = {0};

    // Sub 0
    raw_cmpr[0] = 0xF8; raw_cmpr[1] = 0x00; // c0 = Red
    raw_cmpr[2] = 0x00; raw_cmpr[3] = 0x1F; // c1 = Blue
    raw_cmpr[4] = 0x00; // row 0: all ci=0
    raw_cmpr[5] = 0x55; // row 1: all ci=1
    raw_cmpr[6] = 0xAA; // row 2: all ci=2
    raw_cmpr[7] = 0xFF; // row 3: all ci=3

    // Sub 1
    raw_cmpr[8] = 0x00;  raw_cmpr[9] = 0x1F;  // c0 = Blue
    raw_cmpr[10] = 0xF8; raw_cmpr[11] = 0x00; // c1 = Red (c0 < c1)
    raw_cmpr[12] = 0xFF; // row 0: all ci=3 -> transparent
    raw_cmpr[13] = 0x00;
    raw_cmpr[14] = 0x00;
    raw_cmpr[15] = 0x00;

    uint8_t dst[8 * 8 * 4];
    std::memset(dst, 0, sizeof(dst));

    gc_decode_cmpr(raw_cmpr, dst, 8, 8);

    check_pixel(&dst[(0 * 8 + 0) * 4], 255, 0, 0, 255, "CMPR p0", 0, 0);
    check_pixel(&dst[(1 * 8 + 1) * 4], 0, 0, 255, 255, "CMPR p1", 1, 1);
    check_pixel(&dst[(2 * 8 + 2) * 4], 170, 0, 85, 255, "CMPR p2", 2, 2);
    check_pixel(&dst[(3 * 8 + 3) * 4], 85, 0, 170, 255, "CMPR p3", 3, 3);

    // Sub 1 transparent pixel at (4, 0)
    check_pixel(&dst[(0 * 8 + 4) * 4], 0, 0, 0, 0, "CMPR transp", 4, 0);
}

static void test_ci8_rgb5a3(void) {
    // 8x4 tile = 32 pixels. Each byte is a palette index.
    uint8_t raw_ci8[32] = {0};
    raw_ci8[0] = 0; // index 0
    raw_ci8[1] = 1; // index 1

    // TLUT (RGB5A3 BE):
    // entry 0: 0xFC00 (Red)
    // entry 1: 0x801F (Blue)
    const uint8_t raw_tlut[4] = {
        0xFC, 0x00,
        0x80, 0x1F
    };

    uint8_t dst[8 * 4 * 4];
    std::memset(dst, 0, sizeof(dst));

    gc_decode_ci8_rgb5a3(raw_ci8, dst, 8, 4, raw_tlut, 2, true);

    check_pixel(&dst[(0 * 8 + 0) * 4], 255, 0, 0, 255, "CI8 entry 0", 0, 0);
    check_pixel(&dst[(0 * 8 + 1) * 4], 0, 0, 255, 255, "CI8 entry 1", 1, 0);
}

static void test_rgba8_tile(void) {
    // 4x4 tile = 64 bytes (32 bytes AR, 32 bytes GB)
    uint8_t raw_rgba8[64] = {0};
    // Pixel 0 (i=0): A=200, R=100
    raw_rgba8[0] = 200; // A
    raw_rgba8[1] = 100; // R
    // Pixel 0 GB pass at offset 32: G=50, B=25
    raw_rgba8[32] = 50; // G
    raw_rgba8[33] = 25; // B

    uint8_t dst[4 * 4 * 4];
    std::memset(dst, 0, sizeof(dst));

    gc_decode_rgba8(raw_rgba8, dst, 4, 4);

    check_pixel(&dst[0], 100, 50, 25, 200, "RGBA8 pixel 0", 0, 0);
}

int main(void) {
    test_rgb5a3_tile();
    test_cmpr_block();
    test_ci8_rgb5a3();
    test_rgba8_tile();

    std::printf("check_gc_tex_decode: ALL PASS (ponytail)\n");
    return 0;
}
