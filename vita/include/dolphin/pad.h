#ifndef _DOLPHIN_PAD_H
#define _DOLPHIN_PAD_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAD_MAX_CONTROLLERS 4

#define PAD_BUTTON_LEFT  (1 << 0)
#define PAD_BUTTON_RIGHT (1 << 1)
#define PAD_BUTTON_DOWN  (1 << 2)
#define PAD_BUTTON_UP    (1 << 3)
#define PAD_TRIGGER_Z    (1 << 4)
#define PAD_TRIGGER_R    (1 << 5)
#define PAD_TRIGGER_L    (1 << 6)
#define PAD_BUTTON_A     (1 << 8)
#define PAD_BUTTON_B     (1 << 9)
#define PAD_BUTTON_X     (1 << 10)
#define PAD_BUTTON_Y     (1 << 11)
#define PAD_BUTTON_MENU  (1 << 12)
#define PAD_BUTTON_START (1 << 12)

#define PAD_ERR_NONE          0
#define PAD_ERR_NO_CONTROLLER (-1)

typedef struct PADStatus {
    uint16_t button;
    int8_t stickX;
    int8_t stickY;
    int8_t substickX;
    int8_t substickY;
    uint8_t triggerLeft;
    uint8_t triggerRight;
    uint8_t analogA;
    uint8_t analogB;
    int8_t err;
} PADStatus;

void PADRead(PADStatus* status);
void PADClampCircle(PADStatus* status);
/* Rumble: the handheld Vita has no motor, so these are accepted and ignored. */
void PADControlMotor(s32 chan, u32 command);
BOOL PADReset(u32 mask);

typedef void (*PADSamplingCallback)(void);
BOOL PADInit(void);
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);

#ifdef __cplusplus
}
#endif

#endif
