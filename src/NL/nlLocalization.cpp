#include "NL/nlLocalization.h"
#include "NL/nlMemory.h"
#include "NL/nlFile.h"
#include "NL/nlPrint.h"
#ifdef TARGET_VITA
#include "vita_bswap.h"
#endif

#ifndef TARGET_VITA
extern const unsigned short LocalizationTableNotFound[] = L"Localization Table Not Found";
extern const unsigned short MissingLocString[] = L"missing loc string";
#else
extern const unsigned short LocalizationTableNotFound[] = { 'L','o','c','a','l','i','z','a','t','i','o','n',' ','T','a','b','l','e',' ','N','o','t',' ','F','o','u','n','d', 0 };
extern const unsigned short MissingLocString[] = { 'm','i','s','s','i','n','g',' ','l','o','c',' ','s','t','r','i','n','g', 0 };
#endif

const unsigned long nlLocalization::LanguageId[] = {
    0x7A947B29,
    0xA93C2035,
    0xAAAD26B9,
    0xB482A4B5,
    0xBC0FCCA1,
    0x95F1D726,
    0x5F2F5E69,
    0x983D29BB,
    0x00012332,
};

char* nlLocalization::LanguageName[] = {
    "English",
    "French",
    "German",
    "Spanish",
    "Italian",
    "Japanese",
    "UKEnglish",
    "Longest",
    "Bob",
};

const char nlLocalization::Thumbprint[4] = { 'N', 'L', 'O', 'C' };

nlLocalization* g_pLocalization;

/**
 * Offset/Address/Size: 0x0 | 0x802107AC | size: 0x148
 */
unsigned char nlLocalization::Load(nlLanguage Language, bool ingameloc)
{
    m_CurrentLanguage = Language;

    char Filename[64];
    if (ingameloc)
    {
        nlSNPrintf(Filename, 64, "art/fe/%s_game.loc", LanguageName[Language]);
    }
    else
    {
        nlSNPrintf(Filename, 64, "art/fe/%s.loc", LanguageName[Language]);
    }

    unsigned long FileSize;
    m_pFile = (LOCHeader*)nlLoadEntireFile(Filename, &FileSize, 32, AllocateStart);

    if (m_pFile == 0)
    {
        m_LookupTable = 0;
        m_FirstString = 0;
        return 0;
    }

#ifdef TARGET_VITA
    // .loc is a GameCube memory image: u32 header fields + lookup + UTF-16BE strings.
    vita_bswap_region(&m_pFile->Version, 16, SWAP_U32);
#endif

    if (memcmp(m_pFile, Thumbprint, 4) != 0 || m_pFile->Version != 1 || m_pFile->Language != LanguageId[Language])
    {
        nlFree(m_pFile);
        m_pFile = 0;
        return 0;
    }

    m_LookupTable = (StringLookup*)(m_pFile + 1);
    m_FirstString = (unsigned short*)(&m_LookupTable[m_pFile->StringCount]);

#ifdef TARGET_VITA
    vita_bswap_region(m_LookupTable, m_pFile->StringCount * sizeof(StringLookup), SWAP_U32);
    unsigned long strBytes = FileSize - (unsigned long)((char*)m_FirstString - (char*)m_pFile);
    vita_bswap_region(m_FirstString, strBytes & ~1u, SWAP_U16);
#endif
    return 1;
}

/**
 * Offset/Address/Size: 0x148 | 0x802108F4 | size: 0x48
 */
void nlLocalization::Initialize()
{
    nlLocalization* pLocalization = (nlLocalization*)nlMalloc(sizeof(nlLocalization), 8, false);
    if (pLocalization != NULL)
    {
        pLocalization->m_pFile = NULL;
        pLocalization->m_LookupTable = NULL;
        pLocalization->m_FirstString = NULL;
    }
    g_pLocalization = pLocalization;
}
