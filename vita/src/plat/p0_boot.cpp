#include "plat_abi.h"

#include <dolphin/pad.h>
#include <dolphin/os.h>
#include <NL/nlEndian.h>
#include <NL/nlFile.h>
#include <NL/nlFileGC.h>
#include <NL/nlMemory.h>

#include <cstdio>
#include <cstring>

P0BootResult p0_boot(void)
{
    P0BootResult result{};
    result.link_ok = false;
    result.data_ok = p0_mount_data();

    char test_path[256];
    if (!vita_path_resolve("art/test.bin", test_path, sizeof(test_path)) ||
        std::strcmp(test_path, VITA_DATA_ROOT "/art/test.bin") != 0) {
        std::printf("P0 FAIL: path resolve\n");
        return result;
    }

    if (result.data_ok) {
        nlFile* f = nlOpen("common.ini");
        if (f == nullptr) {
            std::printf("P0 FAIL: nlOpen common.ini\n");
            return result;
        }
        unsigned int sz = 0;
        nlFileSize(f, &sz);
        nlSeek(f, 0, 0);
        char test_buf[16] = {};
        nlRead(f, test_buf, sizeof(test_buf) - 1);
        nlClose(f);

        unsigned long entire_sz = 0;
        void* loaded = nlLoadEntireFile("common.ini", &entire_sz, 32, AllocateEnd);
        if (loaded != nullptr) {
            nlFree(loaded);
        }
    }

    void* mem = OSAlloc(64);
    if (mem == nullptr) {
        std::printf("P0 FAIL: OSAlloc\n");
        return result;
    }
    OSFree(mem);

    VitaMat4 scale{};
    vita_mat4_scale(&scale, 2.0f, 2.0f, 2.0f);
    const float in[3] = {1.0f, 0.0f, 0.0f};
    float out[3] = {};
    vita_mat4_mul_pos(out, in, &scale);
    if (out[0] < 1.5f || out[0] > 2.5f) {
        std::printf("P0 FAIL: math\n");
        return result;
    }

    unsigned short swapped = 0;
    nlSwapEndian(0x1234, &swapped);
    if (swapped != 0x3412) {
        std::printf("P0 FAIL: endian swap\n");
        return result;
    }

    gl_ScreenInfo screenInfo{};
    if (!glplatPreStartup() || !glplatStartup(&screenInfo) || !glplatPostStartup()) {
        std::printf("P0 FAIL: glplat\n");
        return result;
    }
    if (screenInfo.ScreenWidth != 960 || screenInfo.ScreenHeight != 544) {
        std::printf("P0 FAIL: screen %dx%d\n", screenInfo.ScreenWidth, screenInfo.ScreenHeight);
        return result;
    }

    InitPlatPad();
    VBlankPadUpdate();
    UpdatePlatPad(0.0f);

    if (!vita_audio_init()) {
        std::printf("P0 FAIL: audio\n");
        return result;
    }

    PADStatus statuses[PAD_MAX_CONTROLLERS]{};
    PADRead(statuses);
    result.pad0_buttons = statuses[0].button;

    glplatBeginFrame();
    glplatEndFrame();
    glplatSendFrame();
    glplatFinish();

    result.link_ok = true;
    std::printf("P0 link OK (data=%s)\n", result.data_ok ? "yes" : "missing ux0:data/smstrikers/common.ini");
    return result;
}
