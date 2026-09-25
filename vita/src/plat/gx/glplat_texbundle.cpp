// Real PTLG and Texture Loader for VitaGL

#include "plat_abi.h"
#include "plat/gc_tex_decode.h"
#include "plat/vita_tex_mgr.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"
#include "NL/glx/glxTexture.h"
#include "vita_bswap.h"

#include <vitaGL.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {

static void vita_render_log(const char* fmt, ...)
{
    FILE* f = std::fopen("ux0:data/smstrikers/_vita_render.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fclose(f);
}

constexpr uint32_t kMagicPTLG = 0x50544C47u;

enum : uint32_t {
    kFmtRGB565 = 0,
    kFmtRGB5A3 = 1,
    kFmtCMPR = 2,
    kFmtRGBA8 = 3,
    kFmtI8 = 4,
    kFmtI4 = 5,
    kFmtA8 = 6,
    kFmtIA8 = 7,
    kFmtCI8 = 8,
};

std::unordered_map<unsigned long, GLuint> g_tex_map;
GLuint s_white_tex = 0;

static uint32_t be32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

static uint16_t be16(const uint8_t* p)
{
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

static size_t level0_bytes(uint32_t fmt, int w, int h)
{
    const size_t pixels = size_t(w) * size_t(h);
    switch (fmt) {
    case kFmtI4:
        return (pixels + 1) / 2;
    case kFmtI8:
    case kFmtA8:
    case kFmtCI8:
        return pixels;
    case kFmtIA8:
    case kFmtRGB565:
    case kFmtRGB5A3:
        return pixels * 2;
    case kFmtRGBA8:
        return pixels * 4;
    case kFmtCMPR:
        return ((size_t(w) + 7) / 8) * ((size_t(h) + 7) / 8) * 32;
    default:
        return 0;
    }
}

static size_t texture_bytes_all_levels(uint32_t fmt, int w, int h, uint32_t levels)
{
    if (levels == 0)
        levels = 1;
    size_t total = 0;
    for (uint32_t i = 0; i < levels; ++i) {
        total += level0_bytes(fmt, w, h);
        w = w > 1 ? w >> 1 : 1;
        h = h > 1 ? h >> 1 : 1;
    }
    return total;
}

static bool decode_level0(uint32_t fmt, const uint8_t* src, size_t avail, int w, int h,
                          uint32_t nent, uint32_t levels, uint8_t* rgba)
{
    const size_t need = level0_bytes(fmt, w, h);
    if (need == 0 || need > avail) {
        return false;
    }
    switch (fmt) {
    case kFmtRGB565:
        gc_decode_rgb565(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtRGB5A3:
        gc_decode_rgb5a3(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtCMPR:
        gc_decode_cmpr(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtRGBA8:
        gc_decode_rgba8(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtI8:
        gc_decode_i8(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtI4:
        gc_decode_i4(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtIA8:
        gc_decode_ia8(src, rgba, size_t(w), size_t(h));
        return true;
    case kFmtA8:
        gc_decode_i8(src, rgba, size_t(w), size_t(h));
        for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
            rgba[i * 4 + 3] = rgba[i * 4];
            rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        }
        return true;
    case kFmtCI8: {
        // Palette sits after ALL mip levels (see glx_MakeTexture), not after level0.
        const size_t tex_bytes = texture_bytes_all_levels(fmt, w, h, levels);
        if (nent == 0 || nent > 256 || avail < tex_bytes + nent * 2) {
            return false;
        }
        uint8_t palette[256][4];
        gc_build_palette_rgb5a3(src + tex_bytes, nent, palette, true);
        gc_decode_ci8(src, rgba, size_t(w), size_t(h), palette);
        return true;
    }
    default:
        return false;
    }
}

} // namespace

void vita_tex_reset(void)
{
    // Textures remain cached in VitaGL
}

int vita_tex_count(void)
{
    return static_cast<int>(g_tex_map.size());
}

bool vita_tex_get(int index, int* w, int* h, const uint8_t** rgba)
{
    (void)index; (void)w; (void)h; (void)rgba;
    return false;
}

GLuint vita_get_texture(unsigned long handle)
{
    if (handle == 0 || handle == 0xFFFFFFFF) {
        return 0;
    }
    auto it = g_tex_map.find(handle);
    if (it != g_tex_map.end()) {
        return it->second;
    }
    return 0;
}

GLuint vita_get_fallback_texture(void)
{
    if (s_white_tex == 0) {
        uint32_t white = 0xFFFFFFFF;
        glGenTextures(1, &s_white_tex);
        glBindTexture(GL_TEXTURE_2D, s_white_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    }
    return s_white_tex;
}

void vita_register_texture(unsigned long handle, GLuint glTex)
{
    g_tex_map[handle] = glTex;
}

bool glplatLoadTextureBundle(const char* filename)
{
    if (filename == nullptr) {
        return false;
    }

    char full[256];
    if (std::strncmp(filename, "art/", 4) == 0) {
        std::snprintf(full, sizeof(full), "%s", filename);
    } else {
        std::snprintf(full, sizeof(full), "art/%s", filename);
    }

    nlFile* file = nlOpen(full);
    if (file == nullptr) {
        std::printf("[GLT] missing %s\n", full);
        return false;
    }
    unsigned int fsz = 0;
    nlFileSize(file, &fsz);
    if (fsz < 0x30) {
        nlClose(file);
        return false;
    }
    auto* buf = static_cast<uint8_t*>(nlMalloc(fsz, 0x20, false));
    if (!buf) {
        nlClose(file);
        return false;
    }
    nlSeek(file, 0, 0);
    nlRead(file, buf, fsz);
    nlClose(file);

    if (be32(buf) != kMagicPTLG) {
        std::printf("[GLT] bad magic %s\n", full);
        nlFree(buf);
        return false;
    }
    const uint32_t n = be32(buf + 4);
    if (n == 0 || n > 1024) {
        nlFree(buf);
        return false;
    }
    const size_t tex_base = 0x20 + size_t(n) * 0x10;
    if (tex_base > fsz) {
        nlFree(buf);
        return false;
    }

    int loaded_count = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = buf + 0x20 + i * 0x10;
        uint32_t hash = be32(e + 0);
        uint32_t off = be32(e + 4);
        uint32_t chunk = be32(e + 8);
        if (chunk < 0x20 || tex_base + off + chunk > fsz) {
            continue;
        }
        const uint8_t* th = buf + tex_base + off;
        const uint32_t levels = be32(th + 0);
        const uint32_t fmt = be32(th + 4);
        const uint16_t w = be16(th + 0x0E);
        const uint16_t h = be16(th + 0x10);
        const uint32_t nent = be32(th + 0x14);

        if (levels == 0 || levels > 12 || fmt > kFmtCI8 || w == 0 || h == 0 || w > 2048 || h > 2048) {
            continue;
        }

        size_t rgba_size = (size_t)w * (size_t)h * 4;
        auto* rgba = static_cast<uint8_t*>(std::malloc(rgba_size));
        if (!rgba) continue;

        if (decode_level0(fmt, th + 0x20, chunk - 0x20, w, h, nent, levels, rgba)) {
            GLuint glTex = 0;
            auto it = g_tex_map.find(hash);
            if (it != g_tex_map.end() && it->second != 0) {
                glTex = it->second;
                glBindTexture(GL_TEXTURE_2D, glTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            } else {
                glGenTextures(1, &glTex);
                glBindTexture(GL_TEXTURE_2D, glTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                g_tex_map[hash] = glTex;
            }
            loaded_count++;
        }
        std::free(rgba);
    }

    vita_render_log("[GLT] %s: loaded %d / %u textures into VitaGL\n", full, loaded_count, n);
    nlFree(buf);
    return loaded_count > 0;
}

void glplatTextureAdd(unsigned long handle, const void* textureData, unsigned long size)
{
    if (!textureData || size < 0x20) {
        return;
    }

    GXTextureHeader hdr;
    std::memcpy(&hdr, textureData, sizeof(GXTextureHeader));
    if (hdr.numLevels > 32) {
        vita_bswap_gx_texture_header(&hdr);
    }

    int tw = hdr.width;
    int thh = hdr.height;
    if (tw <= 0 || thh <= 0 || tw > 2048 || thh > 2048) {
        return;
    }

    uint32_t fmt = (uint32_t)hdr.format;
    uint32_t nent = hdr.numEntries;
    size_t rgba_size = (size_t)tw * (size_t)thh * 4;
    auto* rgba = static_cast<uint8_t*>(std::malloc(rgba_size));
    if (!rgba) return;

    const uint8_t* src = (const uint8_t*)textureData + sizeof(GXTextureHeader);
    size_t avail = size - sizeof(GXTextureHeader);

    if (decode_level0(fmt, src, avail, tw, thh, nent, (uint32_t)hdr.numLevels, rgba)) {
        GLuint glTex = 0;
        auto it = g_tex_map.find(handle);
        if (it != g_tex_map.end() && it->second != 0) {
            glTex = it->second;
            glBindTexture(GL_TEXTURE_2D, glTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, thh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        } else {
            glGenTextures(1, &glTex);
            glBindTexture(GL_TEXTURE_2D, glTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, thh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            g_tex_map[handle] = glTex;
        }
        // Sample center + mean luminance to catch black/wrong CI8 palettes.
        const size_t npx = (size_t)tw * (size_t)thh;
        const size_t mid = (npx / 2) * 4;
        unsigned long sum = 0;
        for (size_t i = 0; i < npx; ++i)
            sum += rgba[i * 4] + rgba[i * 4 + 1] + rgba[i * 4 + 2];
        const unsigned mean = npx ? (unsigned)(sum / (npx * 3)) : 0;
        vita_render_log("[GLT] glplatTextureAdd: 0x%08lx %dx%d fmt=%u levels=%u nent=%u -> tex=%u mean=%u c=(%u,%u,%u,%u)\n",
                        handle, tw, thh, fmt, (unsigned)hdr.numLevels, nent, glTex, mean,
                        rgba[mid], rgba[mid + 1], rgba[mid + 2], rgba[mid + 3]);
        // Dump a few title-screen CI8s once for host inspection.
        if (fmt == kFmtCI8 && (handle == 0xf4580048ul || handle == 0xd57ec0dcul || handle == 0xa9a51f35ul)) {
            char path[96];
            std::snprintf(path, sizeof(path), "ux0:data/smstrikers/tex_%08lx_%dx%d.rgba", handle, tw, thh);
            FILE* df = std::fopen(path, "wb");
            if (df) {
                std::fwrite(rgba, 1, rgba_size, df);
                std::fclose(df);
            }
        }
    } else {
        vita_render_log("[GLT] glplatTextureAdd decode fail: 0x%08lx %dx%d fmt=%u levels=%u nent=%u avail=%zu\n",
                        handle, tw, thh, fmt, (unsigned)hdr.numLevels, nent, avail);
    }
    std::free(rgba);
}

void glplatTextureReplace(unsigned long handle, const void* textureData, unsigned long size)
{
    glplatTextureAdd(handle, textureData, size);
}
