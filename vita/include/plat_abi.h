#ifndef _PLAT_ABI_H
#define _PLAT_ABI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gl_ScreenInfo {
    int ScreenWidth;
    int ScreenHeight;
    int ColourDepth[4];
    int ZDepth;
    int StencilDepth;
    float PixelCentre;
    bool FSAA;
};

/* Phase 3 audio stub — Phase 8 will use sceAudioOutOpenPort */
bool vita_audio_init(void);
void vita_audio_shutdown(void);

/* Phase 2 VFS — paths relative to ux0:data/smstrikers/ */
#define VITA_DATA_ROOT "ux0:data/smstrikers"

bool vita_path_resolve(const char* relative_path, char* out_path, size_t out_len);
bool vita_data_present(void);
bool p0_mount_data(void);
void* vita_file_open(const char* relative_path);
void vita_file_close(void* file);
size_t vita_file_read(void* file, void* buf, size_t n);
long vita_file_size(void* file);
int vita_file_seek(void* file, long offset, int whence);
long vita_file_tell(void* file);

/* Early fatal helpers */
void vita_fs_fatal_missing_common_ini(void);
bool vita_ensure_data_or_fatal(void);

/* Soft mat4 (column-major) — Phase 1/5 math without NL headers */
typedef struct VitaMat4 {
    float m[16];
} VitaMat4;

void vita_mat4_identity(VitaMat4* out);
void vita_mat4_scale(VitaMat4* out, float sx, float sy, float sz);
void vita_mat4_mul_pos(float out[3], const float pos[3], const VitaMat4* m);

typedef struct P0BootResult {
    bool link_ok;
    bool data_ok;
    uint16_t pad0_buttons;
} P0BootResult;

P0BootResult p0_boot(void);

/* Title screen from dump opening.bnr (real game banner art). */
bool title_screen_load(void);
void title_screen_draw(int frames, bool cross_held);
bool title_screen_ready(void);

/* Navigable Vita front-end milestone. */
void menu_screen_draw(int selected, int frames);

/* Unit 7/10: Thin game boot slice */
typedef enum VitaGameBootStatus {
    VITA_BOOT_OK = 0,
    VITA_BOOT_DATA_MISSING = 1,
    VITA_BOOT_CONFIG_ERROR = 2,
    VITA_BOOT_GL_ERROR = 3,
    VITA_BOOT_TEXTURE_ERROR = 4
} VitaGameBootStatus;

VitaGameBootStatus vita_game_boot(void);
const char* vita_game_boot_status_str(VitaGameBootStatus status);

#ifdef __cplusplus
}

/* C++ linkage on purpose: include/NL/gl/glPlat.h and include/NL/platpad.h
   declare these as ordinary C++ functions, so declaring them extern "C" here
   made our definitions mangle differently and never resolve for the game. */
bool glplatPreStartup(void);
bool glplatStartup(struct gl_ScreenInfo* screenInfo);
bool glplatPostStartup(void);
void glplatBeginFrame(void);
void glplatEndFrame(void);
void glplatSendFrame(void);
void glplatFinish(void);
void glplatAbortFrame(void);
bool glplatIsInitialized(void);

void InitPlatPad(void);
void UpdatePlatPad(float dt);
void VBlankPadUpdate(void);

/* Engine GL/texture — real glLoadTextureBundle in glplat_texbundle.cpp */
bool glStartup(void);
bool glLoadTextureBundle(const char* filename);
bool glplatLoadTextureBundle(const char* filename);
int vita_tex_count(void);
bool vita_tex_get(int index, int* w, int* h, const uint8_t** rgba);
void vita_tex_reset(void);

/* ACGC-style GXP blit (precompiled, no libshacccg) */
bool vita_gxp_blit_init(void);
void vita_gxp_draw_fullscreen(unsigned int tex);
typedef struct VitaColorRect {
    float x, y, w, h;
    uint8_t r, g, b, a;
} VitaColorRect;
void vita_gxp_draw_rects(const VitaColorRect* rects, size_t count);
#else
bool glStartup(void);
bool glLoadTextureBundle(const char* filename);
#endif

#endif
