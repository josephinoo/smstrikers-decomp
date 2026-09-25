// Vita menu milestone: platform init -> original banner -> navigable front end.

#include "plat_abi.h"

#include <vitaGL.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

#include <cstdio>
#include <cstring>

namespace {

// Appends, so the file is a boot trace: the last line tells us how far we got
// before a crash, which is the only diagnostic Vita3K gives us on EXC_BAD_ACCESS.
void write_status(const char* msg)
{
    const int fd = sceIoOpen("ux0:data/smstrikers/_vita_status.txt",
                             SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd < 0) {
        return;
    }
    sceIoWrite(fd, msg, std::strlen(msg));
    sceIoClose(fd);
}

} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[VITA] SMS 00.14 - navigable menu\n");
    sceIoRemove("ux0:data/smstrikers/_vita_status.txt");
    write_status("boot\n");

    write_status("pre-startup\n");
    glplatPreStartup();
    struct gl_ScreenInfo si{};
    write_status("vgl-init\n");
    if (!glplatStartup(&si)) {
        write_status("glplat fail\n");
        return 1;
    }
    write_status("vgl-ok\n");
    glplatPostStartup();

    // The GXP blit is an optimisation, not a requirement: without it the menu
    // still renders through the shaderless path.
    write_status("gxp-init\n");
    write_status(vita_gxp_blit_init() ? "gxp-ok\n" : "gxp-off\n");

    write_status("data-check\n");
    if (!vita_data_present()) {
        write_status("NO DATA ux0:data/smstrikers/\n");
        for (;;) {
            glClearColor(0.5f, 0.05f, 0.05f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            vglSwapBuffers(GL_FALSE);
            sceKernelDelayThread(16'667);
        }
    }

    write_status("data-ok\n");
    write_status("title-load\n");
    title_screen_load();
    write_status("title-ok\n");
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    write_status("menu ready\n");

    int selected = 0;
    int frames = 0;
    unsigned int previous = 0;
    bool redraw = true;
    for (;;) {
        SceCtrlData pad{};
        sceCtrlPeekBufferPositive(0, &pad, 1);
        const unsigned int pressed = pad.buttons & ~previous;
        previous = pad.buttons;

        if (pressed & SCE_CTRL_UP) {
            selected = (selected + 6) % 7;
            redraw = true;
        }
        if (pressed & SCE_CTRL_DOWN) {
            selected = (selected + 1) % 7;
            redraw = true;
        }
        if (pressed & (SCE_CTRL_START | SCE_CTRL_SELECT)) {
            break;
        }

        // Redraw unconditionally: the menu is real geometry now, and vitaGL
        // compiles its fixed-function shaders on the first draw, so a
        // redraw-on-input-only loop would leave the pre-compile frame on screen.
        (void)redraw;
        menu_screen_draw(selected, frames++);
        vglSwapBuffers(GL_FALSE);
        sceKernelDelayThread(16'667);
    }
    return 0;
}
