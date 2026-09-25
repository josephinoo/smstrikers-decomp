#include "plat_abi.h"

// Phase 3 audio stub: opens nothing yet / returns true (silence).
// Phase 8 will use sceAudioOutOpenPort (do NOT invent NGS).
bool vita_audio_init(void)
{
    return true;
}

void vita_audio_shutdown(void)
{
}
