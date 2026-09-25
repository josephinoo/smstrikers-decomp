// Stubs for MusyX link surface.
// These are not implementations and simply return neutral values.

#include <musyx/musyx.h>

extern "C" {

bool sndStreamActivate(SND_STREAMID stid) { return false; }
void sndStreamADPCMParameter(SND_STREAMID stid, SND_ADPCMSTREAM_INFO* adpcmInfo) {}
SND_STREAMID sndStreamAllocEx(u8 prio, void* buffer, u32 samples, u32 frq, u8 vol, u8 pan, u8 span, u8 auxa, u8 auxb, u8 studio, u32 flags, u32 (*updateFunction)(void* buffer1, u32 len1, void* buffer2, u32 len2, u32 user), u32 user, SND_ADPCMSTREAM_INFO* adpcmInfo) { return 0; }
void sndStreamARAMUpdate(SND_STREAMID stid, u32 off1, u32 len1, u32 off2, u32 len2) {}
void sndStreamDeactivate(SND_STREAMID stid) {}
void sndStreamFree(SND_STREAMID stid) {}
void sndStreamFrq(SND_STREAMID stid, u32 frq) {}
void sndStreamMixParameterEx(SND_STREAMID stid, u8 vol, u8 pan, u8 span, u8 auxa, u8 auxb) {}
void sndVolume(u8 volume, u16 time, u8 volGroup) {}

// GCStream.h defines this as extern "C" void sndStreamLPFParameter(unsigned long, bool, unsigned long)
void sndStreamLPFParameter(unsigned long stid, bool on, unsigned long freq) {}

} // extern "C"
