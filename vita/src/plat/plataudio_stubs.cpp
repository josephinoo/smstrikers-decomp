// Stubs for PlatAudio and GCAudioStreaming.
// These are not implementations and simply return neutral values.

#include "NL/plat/plataudio.h"
#include "Game/Sys/GCStream.h"

namespace PlatAudio
{
    bool gUsingDolbyProLogic2 = false;

    u32 GetSndIDError() { return 0; }
    bool IsSFXPlaying(unsigned long uVoiceID) { return false; }
    void InitEmitter(unsigned long index) {}
    bool RemoveEmitter(SFXEmitter* pSFXEmitter) { return false; }
    bool RemoveEmitter(unsigned long index) { return false; }
    static SFXEmitter g_DummyEmitter;
    SFXEmitter* GetSFXEmitter(unsigned long index) { return &g_DummyEmitter; }
    SFXEmitter* GetFreeEmitter(unsigned long& index) { return &g_DummyEmitter; }
    SND_VOICEID GetEmitterVoiceID(SFXEmitter* pSFXEmitter) { return 0; }
    bool IsEmitterActive(SFXEmitter* pSFXEmitter) { return false; }
    void Update3DSFXEmitter(SFXEmitter* pSFXEmitter, const nlVector3& position, const nlVector3& direction, float maxVol) {}
    unsigned long Add3DSFXEmitter(const EmitterStartInfo& info) { return 0; }
    void Remove3DSFXListener(SND_LISTENER* pListener) {}
    void Update3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float overallEmitterVol) {}
    void Add3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float frontAudibleDist, float backAudibleDist, float overallEmitterVol, float volPosOffset, bool bUseDoppler, float fSpeedOfSound) {}
    bool SetPitchBendOnSFX(SND_VOICEID uVoiceID, u16 pitch) { return false; }
    bool SetFilterFreqOnSFX(SND_VOICEID uVoiceID, u16 value) { return false; }
    bool SetMIDIControllerVal14Bit(SND_VOICEID uVoiceID, u8 ctrl, u16 value) { return false; }
    void SetVolGroupVolume(u8 volGroup, float fVol, u16 fadeTime) {}
    bool SetSFXVolumeGroup(u32 uSFXID, u8 volGroup) { return false; }
    bool SetSFXReverbVol(unsigned long uVoiceID, float fVol) { return false; }
    void SetSFXVolume(unsigned long uVoiceID, float fVolume) {}
    bool StopSFX(unsigned long uVoiceID) { return false; }
    unsigned long PlaySFX(const SFXStartInfo& info) { return 0; }
    bool UnloadAllSoundGroupsOnStack(AudioFileData& fileData, unsigned long stackEnum) { return true; }
    bool UnloadAllSoundGroups(AudioFileData& fileData) { return true; }
    bool UnloadSoundGroup(AudioFileData& fileData, unsigned long groupEnum) { return true; }
    bool LoadSoundGroup(AudioFileData& fileData, unsigned long groupEnum, unsigned long stackEnum, bool bUseARAMStreamCallback) { return true; }
    void SetupSoundBuffers(AudioFileData& fileData, bool bStream) {}
    void StopAllSound() {}
    void Shutdown() {}
    bool Initialize(bool bUseDPL2) { return true; }
    void PurgeSampleFileBuffer() {}
    bool IsEntireSampleFileInMem() { return false; }
    unsigned char ReadEntireSampleFileIntoMemSync(const char* sampleFile) { return 0; }
    unsigned char ReadEntireSampleFileIntoMem(const char* sampleFile) { return 0; }
    bool UpdateAuxEffectA(MusyXEffectType type, void* auxEffectSettings) { return false; }
    bool AddAuxEffectA(MusyXEffectType type, void* auxEffectSettings, unsigned char studio) { return false; }
    bool ShutdownAuxEffectA() { return false; }
    bool DeactivateDPL2() { return false; }
    bool ActivateDPL2() { return false; }
    void SetOutputMode(MusyXOutputType output) {}
}

void PrintAvailableARAMMemory() {}

// GCAudioStreaming Virtuals
// Note: they are inline in AudioStreamVirtuals.h, but if that header is not included
// everywhere they are instantiated, the linker will ask for them. We provide empty bodies.
namespace GCAudioStreaming {
    __attribute__((used)) MonoAudioStream::~MonoAudioStream() {}
    __attribute__((used)) void MonoAudioStream::Purge() {}
    __attribute__((used)) bool MonoAudioStream::SafeToPurge() { return false; }
    
    __attribute__((used)) void StereoAudioStream::Purge() {}
    __attribute__((used)) bool StereoAudioStream::SafeToPurge() { return false; }
}
