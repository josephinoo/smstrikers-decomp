// ACGC-style precompiled GXP simple blit (no libshacccg / no runtime GLSL).
// Pattern from ACGC-Vita-Port vita branch: vita_load_gxp + vita_get_simple_shader + PCGXVertex VBO.

#include "plat_abi.h"
#include "gxp_simple.h"

#include <vitaGL.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Match ACGC TARGET_VITA PCGXVertex (pc_gx_internal.h)
struct SMSVertex {
    float position[3];
    float normal[3];
    unsigned char color0[4];
    float texcoord[2][2];
};

GLuint load_gxp(GLenum type, const unsigned char* gxp, unsigned int gxp_size)
{
    const unsigned int total = 4 + gxp_size;
    auto* buf = static_cast<unsigned char*>(std::malloc(total));
    if (!buf) {
        return 0;
    }
    buf[0] = buf[1] = buf[2] = buf[3] = 0; // matrix_uniforms_num = 0
    std::memcpy(buf + 4, gxp, gxp_size);

    GLuint shader = glCreateShader(type);
    glShaderBinary(1, &shader, 0, buf, total);
    std::free(buf);

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::printf("[GXP] glShaderBinary failed err=0x%04X size=%u\n", err, gxp_size);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link_simple(GLuint vs, GLuint fs)
{
    if (!vs || !fs) {
        if (vs) {
            glDeleteShader(vs);
        }
        if (fs) {
            glDeleteShader(fs);
        }
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
        char log[256];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::printf("[GXP] link failed: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    // Keep shaders attached (ACGC deletes; one-shot program is fine either way)
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void set_attribs()
{
    const size_t stride = sizeof(SMSVertex);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SMSVertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SMSVertex, normal)));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<void*>(offsetof(SMSVertex, color0)));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SMSVertex, texcoord)));
}

GLuint g_prog = 0;
GLuint g_vbo = 0;
GLint u_proj = -1, u_mv = -1, u_use_tex = -1, u_tc_src = -1;
GLint u_nchans = -1, u_tmtx = -1, u_tgsrc = -1, u_tex0 = -1;

} // namespace

bool vita_gxp_blit_init(void)
{
#if !VITA_ENABLE_GXP
    // Vita3K host-crashes inside glShaderBinary/glLinkProgram on these
    // precompiled GXP blobs, so the emulator build never takes this path.
    // The menu renders through the shaderless clear/scissor path instead.
    return false;
#else
    if (g_prog) {
        return true;
    }
    GLuint vs = load_gxp(GL_VERTEX_SHADER, gxp_vertex, gxp_vertex_size);
    GLuint fs = load_gxp(GL_FRAGMENT_SHADER, gxp_simple_v0, gxp_simple_v0_size);
    g_prog = link_simple(vs, fs);
    if (!g_prog) {
        std::printf("[GXP] simple program unavailable — textured FE disabled\n");
        return false;
    }
    u_proj = glGetUniformLocation(g_prog, "u_projection");
    u_mv = glGetUniformLocation(g_prog, "u_modelview");
    u_use_tex = glGetUniformLocation(g_prog, "u_use_texture0");
    u_tc_src = glGetUniformLocation(g_prog, "u_tev0_tc_src");
    u_nchans = glGetUniformLocation(g_prog, "u_num_chans");
    u_tmtx = glGetUniformLocation(g_prog, "u_texmtx_enable");
    u_tgsrc = glGetUniformLocation(g_prog, "u_texgen_src0");
    u_tex0 = glGetUniformLocation(g_prog, "u_texture0");
    glGenBuffers(1, &g_vbo);
    std::printf("[GXP] simple blit ready prog=%u\n", g_prog);
    return true;
#endif
}

void vita_gxp_draw_fullscreen(GLuint tex)
{
    if (!g_prog || !tex) {
        return;
    }

    SMSVertex verts[6]{};
    auto setv = [&](int i, float x, float y, float u, float v) {
        verts[i].position[0] = x;
        verts[i].position[1] = y;
        verts[i].position[2] = 0.f;
        verts[i].normal[2] = 1.f;
        verts[i].color0[0] = verts[i].color0[1] = verts[i].color0[2] = verts[i].color0[3] = 255;
        verts[i].texcoord[0][0] = u;
        verts[i].texcoord[0][1] = v;
    };
    // NDC fullscreen, UV top-left origin (GC decode)
    setv(0, -1, -1, 0, 1);
    setv(1, 1, -1, 1, 1);
    setv(2, 1, 1, 1, 0);
    setv(3, -1, -1, 0, 1);
    setv(4, 1, 1, 1, 0);
    setv(5, -1, 1, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    set_attribs();

    glUseProgram(g_prog);
    float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (u_proj >= 0) {
        glUniformMatrix4fv(u_proj, 1, GL_FALSE, ident);
    }
    if (u_mv >= 0) {
        glUniformMatrix4fv(u_mv, 1, GL_FALSE, ident);
    }
    if (u_use_tex >= 0) {
        glUniform1f(u_use_tex, 1.f);
    }
    if (u_tc_src >= 0) {
        glUniform1f(u_tc_src, 0.f);
    }
    if (u_nchans >= 0) {
        glUniform1f(u_nchans, 0.f);
    }
    if (u_tmtx >= 0) {
        glUniform1f(u_tmtx, 0.f);
    }
    if (u_tgsrc >= 0) {
        glUniform1f(u_tgsrc, 0.f);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (u_tex0 >= 0) {
        glUniform1i(u_tex0, 0);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glViewport(0, 0, 960, 544);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDepthMask(GL_TRUE);
}

void vita_gxp_draw_rects(const VitaColorRect* rects, size_t count)
{
    if (rects == nullptr || count == 0) {
        return;
    }

    if (!g_prog) {
        // No GXP program: draw the rects as real geometry through vitaGL's
        // fixed-function pipeline. A scissored glClear per rect would be
        // simpler, but Vita3K's Vulkan renderer does not implement the GXM
        // mask, so every clear wipes the whole framebuffer.
        auto* pos = static_cast<GLfloat*>(std::calloc(count * 18, sizeof(GLfloat)));
        auto* col = static_cast<GLubyte*>(std::calloc(count * 24, sizeof(GLubyte)));
        if (!pos || !col) {
            std::free(pos);
            std::free(col);
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            const VitaColorRect& r = rects[i];
            const GLfloat quad[6][2] = {
                {r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h},
                {r.x, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h},
            };
            for (int j = 0; j < 6; ++j) {
                // vitaGL's fixed-function path wants three-component positions.
                pos[i * 18 + j * 3 + 0] = quad[j][0];
                pos[i * 18 + j * 3 + 1] = quad[j][1];
                pos[i * 18 + j * 3 + 2] = 0.f;
                col[i * 24 + j * 4 + 0] = r.r;
                col[i * 24 + j * 4 + 1] = r.g;
                col[i * 24 + j * 4 + 2] = r.b;
                col[i * 24 + j * 4 + 3] = 255;
            }
        }

        glDisable(GL_TEXTURE_2D);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, pos);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, col);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count * 6));
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        std::free(pos);
        std::free(col);
        return;
    }

    auto* verts = static_cast<SMSVertex*>(std::calloc(count * 6, sizeof(SMSVertex)));
    if (!verts) {
        return;
    }
    size_t vi = 0;
    for (size_t i = 0; i < count; ++i) {
        const VitaColorRect& r = rects[i];
        const float x0 = r.x / 960.f * 2.f - 1.f;
        const float x1 = (r.x + r.w) / 960.f * 2.f - 1.f;
        const float y0 = 1.f - r.y / 544.f * 2.f;
        const float y1 = 1.f - (r.y + r.h) / 544.f * 2.f;
        const float xy[6][2] = {{x0,y1},{x1,y1},{x1,y0},{x0,y1},{x1,y0},{x0,y0}};
        for (int j = 0; j < 6; ++j, ++vi) {
            verts[vi].position[0] = xy[j][0];
            verts[vi].position[1] = xy[j][1];
            verts[vi].normal[2] = 1.f;
            verts[vi].color0[0] = r.r;
            verts[vi].color0[1] = r.g;
            verts[vi].color0[2] = r.b;
            verts[vi].color0[3] = r.a;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, count * 6 * sizeof(SMSVertex), verts, GL_STREAM_DRAW);
    std::free(verts);
    set_attribs();
    glUseProgram(g_prog);
    const float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    if (u_proj >= 0) glUniformMatrix4fv(u_proj, 1, GL_FALSE, ident);
    if (u_mv >= 0) glUniformMatrix4fv(u_mv, 1, GL_FALSE, ident);
    if (u_use_tex >= 0) glUniform1f(u_use_tex, 0.f);
    if (u_tc_src >= 0) glUniform1f(u_tc_src, 0.f);
    if (u_nchans >= 0) glUniform1f(u_nchans, 1.f);
    if (u_tmtx >= 0) glUniform1f(u_tmtx, 0.f);
    if (u_tgsrc >= 0) glUniform1f(u_tgsrc, 0.f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glViewport(0, 0, 960, 544);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count * 6));
    glDepthMask(GL_TRUE);
}
