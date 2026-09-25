#include "Game/Triggers/BinaryTriggerFile.h"

#include "NL/nlFile.h"

/**
 * Offset/Address/Size: 0x0 | 0x802142DC | size: 0x78
 */
BinaryTriggerFile::BinaryTriggerFile(const char* FileName)
{
    m_pFileData = NULL;
    m_pCurrentAnim = 0;
    m_CurrentTrigger = 0;
    m_pFileData = (FILE_HEADER*)nlLoadEntireFile(FileName, &m_FileSize, 0x20, AllocateEnd);
#ifdef TARGET_VITA
    m_pFileData->Version = __builtin_bswap16(m_pFileData->Version);
    m_pFileData->AnimCount = __builtin_bswap16(m_pFileData->AnimCount);
    m_pFileData->BytecodeOffset = __builtin_bswap16(m_pFileData->BytecodeOffset);
    m_pFileData->reserved = __builtin_bswap16(m_pFileData->reserved);
#endif
    m_pFirstAnim = (ANIM_RECORD*)((u8*)m_pFileData + sizeof(FILE_HEADER));
    m_pFirstTrigger = (TRIGGER_RECORD*)((u8*)m_pFirstAnim + (m_pFileData->AnimCount * sizeof(ANIM_RECORD)));
#ifdef TARGET_VITA
    for (u32 i = 0; i < m_pFileData->AnimCount; ++i)
    {
        m_pFirstAnim[i].hash = __builtin_bswap32(m_pFirstAnim[i].hash);
        m_pFirstAnim[i].TriggerCount = __builtin_bswap16(m_pFirstAnim[i].TriggerCount);
        m_pFirstAnim[i].TriggerOffset = __builtin_bswap16(m_pFirstAnim[i].TriggerOffset);
    }

    const u32 triggerBytes = m_pFileData->BytecodeOffset -
        (u32)((u8*)m_pFirstTrigger - (u8*)m_pFileData);
    for (u32 i = 0; i < triggerBytes / sizeof(TRIGGER_RECORD); ++i)
    {
        u32* frame = (u32*)&m_pFirstTrigger[i].Frame;
        *frame = __builtin_bswap32(*frame);
        m_pFirstTrigger[i].Trigger = __builtin_bswap32(m_pFirstTrigger[i].Trigger);
        m_pFirstTrigger[i].ScriptFuncOffset = __builtin_bswap32(m_pFirstTrigger[i].ScriptFuncOffset);
    }
#endif
}
