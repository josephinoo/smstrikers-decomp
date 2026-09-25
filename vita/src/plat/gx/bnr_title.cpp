#include "plat_abi.h"
#include "plat/gc_tex_decode.h"

#include <vitaGL.h>
#include "gxp_simple.h"
#include <vector>

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace {

constexpr int kBnrW = 96;
constexpr int kBnrH = 32;
constexpr size_t kBnrImgBytes = 0x1800;
constexpr int kScale = 3;

// ponytail: stock Vita3K + libvitaGL crash inside glLinkProgram on ACGC GXP
// (MemoryRead @ bad ptr). ACGC uses Brendonm17/vitaGL @async-compressed-tex-prep.
// Keep VS+FS path for that fork / hardware; scissor RLE works without shaders.
constexpr bool kTryGxpBlit = false;

uint8_t* g_rgba = nullptr;
bool g_ready = false;
GLuint g_tex = 0;
GLuint g_prog = 0;

static GLuint load_gxp(GLenum type, const unsigned char* gxp, unsigned int gxp_size)
{
    unsigned int total = 4 + gxp_size;
    std::vector<uint8_t> buf(total, 0);
    std::memcpy(buf.data() + 4, gxp, gxp_size);
    GLuint shader = glCreateShader(type);
    glShaderBinary(1, &shader, 0, buf.data(), static_cast<GLsizei>(total));
    if (glGetError() != GL_NO_ERROR) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_simple(GLuint vs, GLuint fs)
{
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_color0");
    glBindAttribLocation(prog, 3, "a_texcoord0");
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void draw_rgba_scissor_rle(int x0, int y0)
{
    // OpenGL scissor origin is bottom-left; our y0 is top-down like glOrtho(0,960,544,0).
    glEnable(GL_SCISSOR_TEST);
    for (int py = 0; py < kBnrH; ++py) {
        int run_x = 0;
        uint8_t run_c[4];
        std::memcpy(run_c, g_rgba + (py * kBnrW) * 4, 4);
        for (int px = 1; px <= kBnrW; ++px) {
            const uint8_t* c = (px < kBnrW) ? (g_rgba + (py * kBnrW + px) * 4) : nullptr;
            const bool same = c && std::memcmp(c, run_c, 4) == 0;
            if (same) {
                continue;
            }
            if (run_c[3] > 8) {
                const int sx = x0 + run_x * kScale;
                const int sw = (px - run_x) * kScale;
                const int sy_top = y0 + py * kScale;
                const int sh = kScale;
                const int sy = 544 - sy_top - sh;
                glScissor(sx, sy, sw, sh);
                glClearColor(run_c[0] / 255.f, run_c[1] / 255.f, run_c[2] / 255.f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            if (px < kBnrW) {
                run_x = px;
                std::memcpy(run_c, c, 4);
            }
        }
    }
    glDisable(GL_SCISSOR_TEST);
}

} // namespace

bool title_screen_load(void)
{
    g_ready = false;
    std::free(g_rgba);
    g_rgba = nullptr;

    void* f = vita_file_open("opening.bnr");
    if (f == nullptr) {
        std::printf("title: opening.bnr missing\n");
        return false;
    }
    const long sz = vita_file_size(f);
    if (sz < static_cast<long>(0x20 + kBnrImgBytes)) {
        vita_file_close(f);
        return false;
    }
    auto* buf = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(sz)));
    if (buf == nullptr) {
        vita_file_close(f);
        return false;
    }
    std::fseek(static_cast<FILE*>(f), 0, SEEK_SET);
    vita_file_read(f, buf, static_cast<size_t>(sz));
    vita_file_close(f);

    if (std::memcmp(buf, "BNR1", 4) != 0 && std::memcmp(buf, "BNR2", 4) != 0) {
        std::free(buf);
        return false;
    }

    g_rgba = static_cast<uint8_t*>(std::malloc(kBnrW * kBnrH * 4));
    if (g_rgba == nullptr) {
        std::free(buf);
        return false;
    }
    gc_decode_rgb5a3(buf + 0x20, g_rgba, static_cast<size_t>(kBnrW), static_cast<size_t>(kBnrH));
    std::free(buf);
    g_ready = true;

    if (kTryGxpBlit) {
        if (g_tex == 0) {
            glGenTextures(1, &g_tex);
        }
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kBnrW, kBnrH, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (g_prog == 0) {
            GLuint vs = load_gxp(GL_VERTEX_SHADER, gxp_vertex, gxp_vertex_size);
            GLuint fs = load_gxp(GL_FRAGMENT_SHADER, gxp_simple_v0, gxp_simple_v0_size);
            g_prog = link_simple(vs, fs);
            if (fs) glDeleteShader(fs);
            if (!g_prog) {
                std::printf("title: GXP link failed — scissor fallback\n");
            }
        }
    }

    std::printf("title: opening.bnr OK prog=%u gxp=%d\n", g_prog, kTryGxpBlit ? 1 : 0);
    return true;
}

void title_screen_draw(int frames, bool cross_held)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(cross_held ? 0.10f : 0.04f, cross_held ? 0.18f : 0.22f, cross_held ? 0.45f : 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!g_ready || g_rgba == nullptr) {
        return;
    }

    const int logo_w = kBnrW * kScale;
    const int logo_h = kBnrH * kScale;
    const int x0 = (960 - logo_w) / 2;
    const int y0 = (544 - logo_h) / 2 - 40;

    if (g_prog) {
        struct TitVtx {
            float position[3];
            float normal[3];
            unsigned char color0[4];
            float texcoord[2][2];
        };
        auto to_ndc = [](int px, int py, float* ox, float* oy) {
            *ox = static_cast<float>(px) / 960.f * 2.f - 1.f;
            *oy = 1.f - static_cast<float>(py) / 544.f * 2.f;
        };
        TitVtx verts[4];
        std::memset(verts, 0, sizeof(verts));
        float nx0, ny0, nx1, ny1;
        to_ndc(x0, y0, &nx0, &ny0);
        to_ndc(x0 + logo_w, y0 + logo_h, &nx1, &ny1);
        auto fill = [](TitVtx& v, float x, float y, float u, float vv) {
            v.position[0] = x;
            v.position[1] = y;
            v.normal[2] = 1.f;
            v.color0[0] = v.color0[1] = v.color0[2] = v.color0[3] = 255;
            v.texcoord[0][0] = u;
            v.texcoord[0][1] = vv;
        };
        fill(verts[0], nx0, ny0, 0.f, 0.f);
        fill(verts[1], nx1, ny0, 1.f, 0.f);
        fill(verts[2], nx0, ny1, 0.f, 1.f);
        fill(verts[3], nx1, ny1, 1.f, 1.f);

        static GLuint s_vbo = 0;
        if (!s_vbo) glGenBuffers(1, &s_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);

        const GLsizei stride = static_cast<GLsizei>(sizeof(TitVtx));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(TitVtx, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(TitVtx, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<void*>(offsetof(TitVtx, color0)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(TitVtx, texcoord)));

        glUseProgram(g_prog);
        static struct {
            GLuint shader;
            GLint proj, mv, use_tex, tc_src, nchans, tmtx, tgsrc, tex0;
        } ul{};
        if (ul.shader != g_prog) {
            ul.shader = g_prog;
            ul.proj = glGetUniformLocation(g_prog, "u_projection");
            ul.mv = glGetUniformLocation(g_prog, "u_modelview");
            ul.use_tex = glGetUniformLocation(g_prog, "u_use_texture0");
            ul.tc_src = glGetUniformLocation(g_prog, "u_tev0_tc_src");
            ul.nchans = glGetUniformLocation(g_prog, "u_num_chans");
            ul.tmtx = glGetUniformLocation(g_prog, "u_texmtx_enable");
            ul.tgsrc = glGetUniformLocation(g_prog, "u_texgen_src0");
            ul.tex0 = glGetUniformLocation(g_prog, "u_texture0");
        }
        const float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (ul.proj >= 0) glUniformMatrix4fv(ul.proj, 1, GL_FALSE, ident);
        if (ul.mv >= 0) glUniformMatrix4fv(ul.mv, 1, GL_FALSE, ident);
        if (ul.use_tex >= 0) glUniform1f(ul.use_tex, 1.f);
        if (ul.tc_src >= 0) glUniform1f(ul.tc_src, 0.f);
        if (ul.nchans >= 0) glUniform1f(ul.nchans, 0.f);
        if (ul.tmtx >= 0) glUniform1f(ul.tmtx, 0.f);
        if (ul.tgsrc >= 0) glUniform1f(ul.tgsrc, 0.f);
        if (ul.tex0 >= 0) glUniform1i(ul.tex0, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    } else {
        draw_rgba_scissor_rle(x0, y0);
    }

    if ((frames / 30) % 2 == 0) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(460, 100, 40, 16);
        glClearColor(0.95f, 0.85f, 0.15f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
}

bool title_screen_ready(void)
{
    return g_ready;
}
