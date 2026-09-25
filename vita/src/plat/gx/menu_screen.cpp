#include "plat_abi.h"

#include <vitaGL.h>

#include <cctype>
#include <vector>

namespace {

constexpr const char* kItems[] = {
    "FRIENDLY", "CUP BATTLES", "SUPER CUP", "TOURNAMENT",
    "STRIKERS 101", "TROPHIES", "OPTIONS"
};

std::vector<VitaColorRect> g_rects;

// Five-column bitmap font. Each byte is one row, most significant five bits used.
struct Glyph { char c; unsigned char rows[7]; };
constexpr Glyph kGlyphs[] = {
    {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C',{0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}}, {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G',{0x0F,0x10,0x10,0x13,0x11,0x11,0x0F}}, {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I',{0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}}, {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}}, {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}}, {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}}, {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}}
};

void rect(int x, int y, int w, int h, float r, float g, float b)
{
    g_rects.push_back({static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h),
                       static_cast<uint8_t>(r * 255.f), static_cast<uint8_t>(g * 255.f),
                       static_cast<uint8_t>(b * 255.f), 255});
}

const Glyph* glyph(char c)
{
    for (const auto& g : kGlyphs) if (g.c == c) return &g;
    return nullptr;
}

void text(const char* value, int x, int y, int scale, float r, float g, float b)
{
    for (; *value; ++value, x += 6 * scale) {
        const Glyph* gph = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(*value))));
        if (!gph) continue;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (gph->rows[row] & (1 << (4 - col)))
                    rect(x + col * scale, y + row * scale, scale, scale, r, g, b);
    }
}

} // namespace

void menu_screen_draw(int selected, int frames)
{
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 960, 544, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const float pulse = ((frames / 30) & 1) ? 0.08f : 0.f;
    glClearColor(0.015f, 0.025f + pulse, 0.055f + pulse, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    g_rects.clear();

    // The shaderless banner fallback needs thousands of clears per frame on
    // Vita3K. Keep the header cheap until the .fen/GXP renderer lands.
    rect(55, 75, 430, 118, 0.08f, 0.012f, 0.018f);
    rect(68, 88, 404, 92, 0.72f, 0.07f, 0.025f);
    text("SUPER MARIO", 96, 103, 5, 1.f, 0.86f, 0.18f);
    text("STRIKERS", 148, 148, 5, 1.f, 1.f, 1.f);

    rect(545, 72, 340, 414, 0.015f, 0.02f, 0.035f);
    text("MAIN MENU", 612, 92, 4, 0.95f, 0.75f, 0.12f);
    for (int i = 0; i < 7; ++i) {
        const int y = 155 + i * 43;
        if (i == selected) {
            rect(570, y - 10, 285, 34, 0.65f, 0.08f, 0.025f);
            rect(578, y - 2, 6, 18, 1.f, 0.84f, 0.15f);
        }
        text(kItems[i], 596, y, 3, 1.f, i == selected ? 0.9f : 0.72f, i == selected ? 0.25f : 0.68f);
    }
    text("UP DOWN  START EXIT", 580, 462, 2, 0.7f, 0.72f, 0.78f);
    vita_gxp_draw_rects(g_rects.data(), g_rects.size());
}
