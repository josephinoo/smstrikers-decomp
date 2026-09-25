#include "plat_abi.h"

#include <dolphin/pad.h>

#include <psp2/ctrl.h>

#include <cstring>

namespace {

PADStatus g_pad_a[PAD_MAX_CONTROLLERS]{};
PADStatus g_pad_b[PAD_MAX_CONTROLLERS]{};
PADStatus* g_current = g_pad_a;
PADStatus* g_next = g_pad_b;
bool g_pad_inited = false;

void sample_into(PADStatus* out)
{
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        PADStatus& status = out[i];
        std::memset(&status, 0, sizeof(status));
        status.err = static_cast<int8_t>(PAD_ERR_NO_CONTROLLER);

        if (i != 0) {
            continue;
        }

        SceCtrlData pad{};
        sceCtrlPeekBufferPositive(0, &pad, 1);
        status.err = PAD_ERR_NONE;

        if (pad.buttons & SCE_CTRL_LEFT) {
            status.button |= PAD_BUTTON_LEFT;
        }
        if (pad.buttons & SCE_CTRL_RIGHT) {
            status.button |= PAD_BUTTON_RIGHT;
        }
        if (pad.buttons & SCE_CTRL_DOWN) {
            status.button |= PAD_BUTTON_DOWN;
        }
        if (pad.buttons & SCE_CTRL_UP) {
            status.button |= PAD_BUTTON_UP;
        }
        if (pad.buttons & SCE_CTRL_CROSS) {
            status.button |= PAD_BUTTON_A;
        }
        if (pad.buttons & SCE_CTRL_CIRCLE) {
            status.button |= PAD_BUTTON_B;
        }
        if (pad.buttons & SCE_CTRL_SQUARE) {
            status.button |= PAD_BUTTON_X;
        }
        if (pad.buttons & SCE_CTRL_TRIANGLE) {
            status.button |= PAD_BUTTON_Y;
        }
        if (pad.buttons & SCE_CTRL_START) {
            status.button |= PAD_BUTTON_START;
        }
        if (pad.buttons & SCE_CTRL_SELECT) {
            status.button |= PAD_TRIGGER_Z;
        }
        if (pad.buttons & SCE_CTRL_LTRIGGER) {
            status.button |= PAD_TRIGGER_L;
        }
        if (pad.buttons & SCE_CTRL_RTRIGGER) {
            status.button |= PAD_TRIGGER_R;
        }

        status.stickX = static_cast<int8_t>(pad.lx - 128);
        status.stickY = static_cast<int8_t>(127 - pad.ly);
        status.substickX = static_cast<int8_t>(pad.rx - 128);
        status.substickY = static_cast<int8_t>(127 - pad.ry);
    }
}

} // namespace

void PADRead(PADStatus* status)
{
    if (status == nullptr) {
        return;
    }
    if (!g_pad_inited) {
        // Sample directly rather than calling InitPlatPad(): NL's own
        // InitPlatPad in src/NL/plat/platpad.cpp calls straight back into
        // PADRead, and the two would recurse until the stack ran out.
        sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
        sample_into(g_pad_a);
        sample_into(g_pad_b);
        g_pad_inited = true;
    }
    std::memcpy(status, g_current, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);
}

void PADClampCircle(PADStatus* /*status*/)
{
    // ponytail: deadzone later if FE feels sticky
}

// The handheld Vita has no rumble motor and nothing to reset: accept the
// GameCube pad calls and do nothing, so the game's rumble bookkeeping still
// runs and simply produces no vibration.
void PADControlMotor(s32 chan, u32 command)
{
    (void)chan;
    (void)command;
}

BOOL PADReset(u32 mask)
{
    (void)mask;
    return true;
}
