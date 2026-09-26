// glplat_stub.cpp — VitaGL present and packet rendering pipeline

#define glFinish nl_glFinish
#include "NL/gl/gl.h"
#include "NL/gl/glView.h"
#include "NL/gl/glRenderList.h"
#include "NL/gl/glUserData.h"
#include "NL/gl/glMatrix.h"
#include "NL/glx/glxMatrix.h"
#include "NL/glx/glxMemory.h"
#include "Game/GL/GLMeshWriter.h"
#undef glFinish

#include <vitaGL.h>
#include <psp2/kernel/clib.h>
#include "plat_abi.h"
#include "plat/vita_tex_mgr.h"
#include <cstdio>
#include <cstdint>

extern "C" int g_rt_scene, g_rt_pres, g_rt_image, g_rt_attach, g_rt_rl_attach, g_rt_rl_fail, g_rt_last_view; // TEMP trace

namespace {

bool s_vgl_initialized = false;
unsigned g_vita_present_frame = 0;

// FE packet lists arrive in reverse painter order (logo before underlay). Queue
// and flush back-to-front so underlay ends up behind the title art.
constexpr int kFeQueueMax = 256;
const glModelPacket* s_fe_queue[kFeQueueMax];
int s_fe_queue_n = 0;
bool s_fe_flushing = false;

}
extern "C" void vita_trace_log(const char* fmt, ...);
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

static void vita_packet_render_cb(eGLView view, unsigned long flags, const glModelPacket* p)
{
    (void)flags;
    if (!p || p->numVertices == 0) {
        return;
    }

    const bool is_fe = (view >= GLV_FrontEnd);
    // Defer FE draws so we can reverse them after the full list is known.
    if (is_fe && !s_fe_flushing) {
        if (s_fe_queue_n < kFeQueueMax) {
            s_fe_queue[s_fe_queue_n++] = p;
        }
        return;
    }

    // 1. Matrix setup
    nlMatrix4 proj;
    glViewGetProjectionMatrix(view, proj);

    nlMatrix4 vmat;
    glViewGetViewMatrix(view, vmat);

    nlMatrix4 mmat;
    if (p->state.matrix != 0 && p->state.matrix != 0xFFFFFFFF) {
        glplatGetMatrix(p->state.matrix, mmat);
    } else {
        mmat.SetIdentity();
    }

    nlMatrix4 mv;
    nlMultMatrices(mv, mmat, vmat);

    // GX C_MTXOrtho → GL column-major gives clip_w≈-z_eye (601 at z=-600),
    // collapsing FE quads after perspective divide. Use real glOrtho for FE.
    float gl_mv[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            gl_mv[c * 4 + r] = mv.e2[c][r];
        }
    }

    glMatrixMode(GL_PROJECTION);
    if (is_fe) {
        // Explicit GL ortho (vitaGL glOrtho has been flaky with negative near).
        const float L = -320.f, R = 320.f, B = -240.f, T = 240.f, N = -5000.f, F = 5000.f;
        const float proj_gl[16] = {
            2.f / (R - L), 0, 0, 0,
            0, 2.f / (T - B), 0, 0,
            0, 0, -2.f / (F - N), 0,
            -(R + L) / (R - L), -(T + B) / (T - B), -(F + N) / (F - N), 1.f
        };
        glLoadMatrixf(proj_gl);
    } else {
        float gl_proj[16];
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                gl_proj[c * 4 + r] = proj.e2[c][r];
            }
        }
        glLoadMatrixf(gl_proj);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(gl_mv);

    // 2. Texture setup
    GLuint glTex = 0;
    if (p->state.texture[0] != 0 && p->state.texture[0] != 0xFFFFFFFF) {
        glTex = vita_get_texture(p->state.texture[0]);
        if (glTex == 0) {
            glTex = vita_get_fallback_texture();
        }
    }

    if (glTex != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, glTex);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    // 3. States — FE/Anark (and above) are 2D overlays
    if (is_fe) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // MODULATE with constant white RGB (colour array skipped below) so a bad
        // endian colour stream can't zero CI8 logo RGB; alpha still fades.
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    // 4. Extract vertex streams
    static const int MAX_V = 4096;
    int numV = p->numVertices;
    if (numV > MAX_V) numV = MAX_V;

    static float pos_arr[MAX_V * 3];
    static float uv_arr[MAX_V * 2];
    static uint8_t col_arr[MAX_V * 4];
    bool has_pos = false;
    bool has_col = false;
    bool has_uv = false;

    for (int s = 0; s < p->numStreams; ++s) {
        const glModelStream& stream = p->streams[s];
        if (stream.address == 0) continue;

        if (stream.id == GLStream_Position) {
            has_pos = true;
            if (stream.stride == 12) {
                const float* src = (const float*)stream.address;
                for (int i = 0; i < numV; ++i) {
                    pos_arr[i * 3 + 0] = src[i * 3 + 0];
                    pos_arr[i * 3 + 1] = src[i * 3 + 1];
                    pos_arr[i * 3 + 2] = src[i * 3 + 2];
                }
            } else if (stream.stride == 6) {
                const int16_t* src = (const int16_t*)stream.address;
                for (int i = 0; i < numV; ++i) {
                    pos_arr[i * 3 + 0] = src[i * 3 + 0] / 256.0f;
                    pos_arr[i * 3 + 1] = src[i * 3 + 1] / 256.0f;
                    pos_arr[i * 3 + 2] = src[i * 3 + 2] / 256.0f;
                }
            }
        } else if (stream.id == GLStream_Position4) {
            has_pos = true;
            const float* src = (const float*)stream.address;
            for (int i = 0; i < numV; ++i) {
                pos_arr[i * 3 + 0] = src[i * 4 + 0];
                pos_arr[i * 3 + 1] = src[i * 4 + 1];
                pos_arr[i * 3 + 2] = src[i * 4 + 2];
            }
        } else if (stream.id == GLStream_Colour) {
            has_col = true;
            const uint8_t* src = (const uint8_t*)stream.address;
            for (int i = 0; i < numV; ++i) {
                col_arr[i * 4 + 0] = src[i * 4 + 0];
                col_arr[i * 4 + 1] = src[i * 4 + 1];
                col_arr[i * 4 + 2] = src[i * 4 + 2];
                col_arr[i * 4 + 3] = src[i * 4 + 3];
            }
        } else if (stream.id == GLStream_Diffuse) {
            has_uv = true;
            if (stream.stride == 4) {
                const int16_t* src = (const int16_t*)stream.address;
                for (int i = 0; i < numV; ++i) {
                    uv_arr[i * 2 + 0] = src[i * 2 + 0] / 1024.0f;
                    uv_arr[i * 2 + 1] = src[i * 2 + 1] / 1024.0f;
                }
            } else if (stream.stride == 8) {
                const float* src = (const float*)stream.address;
                for (int i = 0; i < numV; ++i) {
                    uv_arr[i * 2 + 0] = src[i * 2 + 0];
                    uv_arr[i * 2 + 1] = src[i * 2 + 1];
                }
            }
        }
    }

    if (!has_pos) {
        return;
    }

    // First 8 + more once menu FE attaches (attach climbs past ~500).
    static int s_draw_count = 0;
    bool log_this = (s_draw_count < 8) || (::g_rt_attach > 500 && s_draw_count < 80);
    if (log_this) {
        vita_render_log("[DRAW #%d] scene=%d view=%d numV=%d prim=%d tex=0x%08lx glTex=%u col=%02x%02x%02x%02x uv0=(%.2f,%.2f) pos0=(%.1f,%.1f) scl=(%.2f,%.2f) tr=(%.1f,%.1f,%.1f)\n",
                        s_draw_count, ::g_rt_scene, (int)view, numV, (int)p->primType, p->state.texture[0], glTex,
                        has_col ? col_arr[0] : 0xff, has_col ? col_arr[1] : 0xff, has_col ? col_arr[2] : 0xff, has_col ? col_arr[3] : 0xff,
                        has_uv ? uv_arr[0] : 0.f, has_uv ? uv_arr[1] : 0.f,
                        pos_arr[0], pos_arr[1],
                        mmat.e2[0][0], mmat.e2[1][1],
                        mmat.e2[3][0], mmat.e2[3][1], mmat.e2[3][2]);
        s_draw_count++;
    }

    // 5. Draw — expand quads to tris (vitaGL GL_QUADS is flaky with tex+color)
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    static float tri_pos[MAX_V * 3 * 2];
    static float tri_uv[MAX_V * 2 * 2];
    static uint8_t tri_col[MAX_V * 4 * 2];
    const float* draw_pos = pos_arr;
    const float* draw_uv = uv_arr;
    const uint8_t* draw_col = col_arr;
    int draw_n = numV;
    GLenum mode = GL_TRIANGLES;

    if (p->primType == GLP_QuadList && (numV % 4) == 0) {
        int out = 0;
        for (int q = 0; q < numV; q += 4) {
            const int idx[6] = { q + 0, q + 1, q + 2, q + 0, q + 2, q + 3 };
            for (int k = 0; k < 6; ++k) {
                const int i = idx[k];
                tri_pos[out * 3 + 0] = pos_arr[i * 3 + 0];
                tri_pos[out * 3 + 1] = pos_arr[i * 3 + 1];
                tri_pos[out * 3 + 2] = pos_arr[i * 3 + 2];
                if (has_uv) {
                    tri_uv[out * 2 + 0] = uv_arr[i * 2 + 0];
                    tri_uv[out * 2 + 1] = uv_arr[i * 2 + 1];
                }
                if (has_col) {
                    tri_col[out * 4 + 0] = col_arr[i * 4 + 0];
                    tri_col[out * 4 + 1] = col_arr[i * 4 + 1];
                    tri_col[out * 4 + 2] = col_arr[i * 4 + 2];
                    tri_col[out * 4 + 3] = col_arr[i * 4 + 3];
                }
                ++out;
            }
        }
        draw_pos = tri_pos;
        draw_uv = tri_uv;
        draw_col = tri_col;
        draw_n = out;
        mode = GL_TRIANGLES;
    } else {
        switch (p->primType) {
            case GLP_TriList:   mode = GL_TRIANGLES; break;
            case GLP_TriStrip:  mode = GL_TRIANGLE_STRIP; break;
            case GLP_TriFan:    mode = GL_TRIANGLE_FAN; break;
            case GLP_QuadList:  mode = GL_TRIANGLES; break;
            case GLP_LineList:  mode = GL_LINES; break;
            case GLP_LineStrip: mode = GL_LINE_STRIP; break;
            default: break;
        }
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, draw_pos);

    // FE uses REPLACE; skip colour array so a bad channel can't zero RGB.
    if (has_col && !is_fe) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, draw_col);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
        if (has_col && is_fe) {
            // Keep per-quad opacity via constant colour alpha only.
            glColor4ub(255, 255, 255, draw_col[3]);
        } else {
            glColor4ub(255, 255, 255, 255);
        }
    }

    if (has_uv && glTex != 0) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, draw_uv);
    } else {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glDrawArrays(mode, 0, draw_n);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

} // namespace

extern "C" void vita_trace_log(const char* fmt, ...)
{
    FILE* f = std::fopen("ux0:data/smstrikers/_vita_render.log", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fclose(f);
}

bool glplatPreStartup(void) { return true; }

bool glplatStartup(struct gl_ScreenInfo* screenInfo)
{
    if (screenInfo != nullptr) {
        screenInfo->ScreenWidth = 960;
        screenInfo->ScreenHeight = 544;
        screenInfo->ColourDepth[0] = screenInfo->ColourDepth[1] = screenInfo->ColourDepth[2] = screenInfo->ColourDepth[3] = 8;
        screenInfo->ZDepth = 24;
        screenInfo->StencilDepth = 0;
        screenInfo->PixelCentre = 0.5f;
        screenInfo->FSAA = false;
    }
    if (s_vgl_initialized) {
        return true;
    }

    // No vitaGL mspace pools: route GPU memory through the (GXM-mapped) newlib
    // heap instead. vitaGL's pools are sceClibMspace heaps, which Vita3K runs as
    // host code; when an allocation writes a chunk header into a page Vita3K has
    // write-protected for a texture, the emulator cannot recover and dies with
    // "Unhandled write protected region was valid" inside mspace_malloc. Newlib's
    // malloc is guest code, so the same write is handled normally.
    vglUseExtraMem(GL_TRUE);
    vglInitWithCustomSizes(0x800000, 960, 544, 0, 0, 0, 0, SCE_GXM_MULTISAMPLE_NONE);

    glViewport(0, 0, 960, 544);
    // UI code uses scissored clears.  A scissor left enabled here would make
    // every subsequent frame clear only that old UI rectangle.
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    s_vgl_initialized = true;
    vita_render_log("glplatStartup: ok\n");
    return true;
}

bool glplatPostStartup(void) { return true; }

void glplatBeginFrame(void)
{
    if (!s_vgl_initialized) {
        glplatStartup(nullptr);
    }
    glViewport(0, 0, 960, 544);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void glplatEndFrame(void) {}

void glplatSendFrame(void)
{
    if (!s_vgl_initialized) {
        return;
    }

    g_vita_present_frame++;

    static int s_frame_num = 0;
    if (s_frame_num < 10 || s_frame_num % 60 == 0) {
        vita_render_log("[FRAME #%d] begin presenting\n", s_frame_num);
    }

    glViewport(0, 0, 960, 544);

    int total_packets = 0;
    for (int v = 0; v < GLV_Num; v++) {
        if (!glViewGetEnable((eGLView)v)) continue;
        GLRenderList* rl = gl_ViewGetRenderList((eGLView)v);
        if (!rl || rl->IsEmpty()) continue;

        if ((eGLView)v >= GLV_FrontEnd) {
            s_fe_queue_n = 0;
            gl_ViewIterate((eGLView)v, vita_packet_render_cb); // queues
            // Painter order: underlay → FX → logo → UI (list arrives reversed).
            auto fe_prio = [](const glModelPacket* pkt) -> int {
                const unsigned long t = pkt->state.texture[0];
                if (t == 0x85b9c71bul) return 0; // title underlay
                if (t == 0xf4580048ul) return 1; // starburst
                if (t == 0xd57ec0dcul) return 2; // mario strikers logo
                return 3; // copyright / text / other
            };
            // Stable-ish insertion by priority (n is small).
            for (int i = 1; i < s_fe_queue_n; ++i) {
                const glModelPacket* key = s_fe_queue[i];
                const int pk = fe_prio(key);
                int j = i - 1;
                while (j >= 0 && fe_prio(s_fe_queue[j]) > pk) {
                    s_fe_queue[j + 1] = s_fe_queue[j];
                    --j;
                }
                s_fe_queue[j + 1] = key;
            }
            s_fe_flushing = true;
            for (int i = 0; i < s_fe_queue_n; ++i) {
                vita_packet_render_cb((eGLView)v, 0, s_fe_queue[i]);
            }
            s_fe_flushing = false;
            s_fe_queue_n = 0;
        } else {
            gl_ViewIterate((eGLView)v, vita_packet_render_cb);
        }
        total_packets++;
    }

    vglSwapBuffers(GL_FALSE);
    glplatFrameAllocNextFrame();

    if (s_frame_num < 10 || s_frame_num % 60 == 0) {
        vita_render_log("[FRAME #%d] swapped, views_drawn=%d\n", s_frame_num, total_packets);
        vita_render_log("  [TRACE] scene=%d pres=%d image=%d attach=%d rl_attach=%d rl_fail=%d last_view=%d\n",
                        g_rt_scene, g_rt_pres, g_rt_image, g_rt_attach, g_rt_rl_attach, g_rt_rl_fail, g_rt_last_view);
    }
    s_frame_num++;
}

void glplatFinish(void) { glFinish(); }
void glplatAbortFrame(void) { glFinish(); }
bool glplatIsInitialized(void) { return s_vgl_initialized; }
