#include "Game/FE/feScene.h"

#include "NL/nlDebug.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"
#include "NL/gl/glMatrix.h"
#include "NL/nlDLRing.h"
#ifdef TARGET_VITA
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/tlComponentInstance.h"
#endif

bool gSebringLoadPackageToVirtualMemory = false;

struct FE_FILE_HEADER
{
    char Thumbprint[4];
    unsigned int Version;
    unsigned int DataLength;
    unsigned int PointerTableLength;
};

class QueueResourceLoadCallback
{
public:
    void Callback(FEResourceHandle*);
    FEResourceManager* m_resourceManager;
};

class UnloadResourceCallback
{
public:
    void Callback(FEResourceHandle*);
    FEResourceManager* m_resourceManager;
};

/**
 * Offset/Address/Size: 0x0 | 0x80209D74 | size: 0x24
 */
void FEScene::Update(float dt)
{
    m_pFEPackage->Update(dt);
}

/**
 * Offset/Address/Size: 0x24 | 0x80209D98 | size: 0x4
 */
void FEScene::AllResourcesLoadedCallback()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x28 | 0x80209D9C | size: 0x24
 */
void QueueResourceLoadCallback::Callback(FEResourceHandle* handle)
{
    m_resourceManager->QueueResourceLoad(handle);
}

/**
 * Offset/Address/Size: 0x4C | 0x80209DC0 | size: 0x70
 */
void FEScene::UnloadPackage()
{
    UnloadResourceCallback unloadResourceCallback;
    unloadResourceCallback.m_resourceManager = FEResourceManager::Instance();
    nlWalkRing<FEResourceHandle, UnloadResourceCallback>(m_pFEPackage->m_pResourceList, &unloadResourceCallback, &UnloadResourceCallback::Callback);
    FEResourceManager::Instance()->UnloadResource(&m_feSceneResourceHandle);
}

/**
 * Offset/Address/Size: 0xBC | 0x80209E30 | size: 0x24
 */
void UnloadResourceCallback::Callback(FEResourceHandle* handle)
{
    m_resourceManager->UnloadResource(handle);
}

static inline void RelocatePointer(unsigned long* pPointer, void* pData)
{
    unsigned long value = *pPointer;
    unsigned long mask = ~((value + 1) | ((unsigned long)-1 - value));
    unsigned long sum = value + (unsigned long)pData;
    mask = (unsigned long)((long)mask >> 31);
    *pPointer = sum & ~mask;
}

/**
 * Offset/Address/Size: 0xE0 | 0x80209E54 | size: 0x26C
 */
#ifdef TARGET_VITA
#include "vita_bswap.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlComponent.h"
#include <set>

// The .fen image is swapped as 32-bit words, which is right for pointers, u32s
// and floats but scrambles narrower fields packed into one word. Put those back.
namespace
{

// A char array: undo the word swap.
void vita_fen_fix_bytes(void* p, unsigned int size)
{
    vita_bswap_u32_array(p, size);
}

// TLInstance 0x7C: u16 m_priority, bool m_bVisible, pad. Word-swapped bytes are
// [o3 o2 o1 o0]; the right little-endian layout is [o1 o0 o2 o3].
void vita_fen_fix_u16_u8_u8(void* p)
{
    unsigned char* b = (unsigned char*)p;
    unsigned char o3 = b[0], o2 = b[1], o1 = b[2], o0 = b[3];
    b[0] = o1;
    b[1] = o0;
    b[2] = o2;
    b[3] = o3;
}

void vita_fen_fix_slides(TLSlide* head, std::set<void*>& seen);

// FELibObjectAttributes packs bool bVisible at 0x30 and a byte-wise nlColour at
// 0x31, straddling two words: restore those 8 bytes as plain bytes.
void vita_fen_fix_attributes(FELibObjectAttributes* attr)
{
    vita_fen_fix_bytes(&attr->bVisible, 8);
}

// FEAnimation::m_cast_type is a u16 in a word with pad. After SWAP_U32, cast=1
// becomes 0 so v3 rings are walked as fAnimationKeyframe — GetStart then reads
// Y.m_fPoint (e.g. 4.8f) as m_next.
void vita_fen_fix_animations(FEAnimation* head, std::set<void*>& seen)
{
    if (head == NULL)
        return;
    FEAnimation* curr = head;
    do
    {
        if (!seen.insert(curr).second)
            break;
        vita_fen_fix_u16_u8_u8(&curr->m_cast_type);
        curr = curr->m_next;
    } while (curr != NULL && curr != head);
}

void vita_fen_fix_libobject(FELibObject* obj, std::set<void*>& seen)
{
    if (obj == NULL || !seen.insert(obj).second)
        return;
    vita_fen_fix_attributes(&obj->m_attributes);
    vita_fen_fix_bytes(obj->m_szName, sizeof(obj->m_szName));
}

// UTF-16 in the fen data blob is scrambled by SWAP_U32: each word's two
// code units swap ("PR"→"RP"). Undo that through the NUL terminator.
// Bound the walk — a bad reloc pointer used to walk off into the heap and
// trash the freelist (saw ~500k Invalid writes @0xfffda224 after main_menuv2).
void vita_fen_fix_utf16(unsigned short* s)
{
    if (s == NULL)
        return;
    unsigned long addr = (unsigned long)s;
    if (addr < 0x81000000u || addr > 0x8F000000u)
        return;
    for (int i = 0; i < 512; ++i)
    {
        unsigned short a = s[0];
        unsigned short b = s[1];
        s[0] = b;
        s[1] = a;
        if (s[0] == 0 || s[1] == 0)
            break;
        s += 2;
    }
}

void vita_fen_fix_instances(TLInstance* head, std::set<void*>& seen)
{
    if (head == NULL)
        return;
    TLInstance* curr = head;
    do
    {
        if (!seen.insert(curr).second)
            break;
        vita_fen_fix_bytes(curr->m_szName, sizeof(curr->m_szName));
        vita_fen_fix_u16_u8_u8(&curr->m_priority);
        vita_fen_fix_attributes(&curr->m_overloadedAttributes);
        vita_fen_fix_libobject(curr->m_component, seen);
        if (curr->m_type == TLAT_TEXT)
        {
            TLTextInstance* text = (TLTextInstance*)curr;
            // EffectColour is 4 bytes packed; u32 swap made AA BB GG RR.
            vita_fen_fix_bytes(&text->m_OverloadedAttributes.EffectColour, 4);
            vita_fen_fix_utf16(const_cast<unsigned short*>(text->m_wcUserString));
        }
        // Only a component instance's library object is a TLComponent with
        // slides of its own. Its m_szName is not fixed: the header's layout
        // there is unreliable and a guess corrupts the following object.
        if (curr->m_type == TLAT_COMPONENT)
            vita_fen_fix_slides(curr->m_component->pChildren, seen);
        vita_fen_fix_instances(curr->pChildren, seen);
        curr = curr->m_next;
    } while (curr != NULL && curr != head);
}

void vita_fen_fix_slides(TLSlide* head, std::set<void*>& seen)
{
    if (head == NULL)
        return;
    TLSlide* curr = head;
    do
    {
        if (!seen.insert(curr).second)
            break;
        vita_fen_fix_bytes(curr->m_szName, sizeof(curr->m_szName));
        vita_fen_fix_instances(curr->m_instances, seen);
        vita_fen_fix_animations(curr->m_animations, seen);
        curr = curr->m_next;
    } while (curr != NULL && curr != head);
}

void vita_fen_fix_package(FEPackage* package)
{
    std::set<void*> seen;
    if (package->m_pFEPresentation != NULL)
        vita_fen_fix_slides(package->m_pFEPresentation->m_slides, seen);
}

} // namespace
#endif

bool FEScene::LoadPackage(const char* szPackageFileName)
{
    nlFile* file;
    FE_FILE_HEADER FenHdr ATTRIBUTE_ALIGN(32);
    void* pData;
    unsigned long* pPointerLocation;
    unsigned long* pLastPointer;
    unsigned long* pCurrentPointer;
    unsigned long* pPointer;

    file = nlOpen(szPackageFileName);
    nlRead(file, &FenHdr, 0x10);

#ifdef TARGET_VITA
    vita_bswap_region(&FenHdr, 0x10, SWAP_U32);
#endif

    if (gSebringLoadPackageToVirtualMemory)
    {
        pData = nlVirtualAlloc(FenHdr.DataLength, false);
        if (pData == NULL)
        {
            nlBreak();
        }
        nlReadToVirtualMemory(file, pData, FenHdr.DataLength, 0x4000);
    }
    else
    {
        pData = nlMalloc(FenHdr.DataLength, 0x20, false);
        nlRead(file, pData, FenHdr.DataLength);
    }

#ifdef TARGET_VITA
    vita_bswap_region(pData, FenHdr.DataLength, SWAP_U32);
#endif

    pPointerLocation = (unsigned long*)nlMalloc(FenHdr.PointerTableLength, 0x20, true);
    nlRead(file, pPointerLocation, FenHdr.PointerTableLength);
    nlClose(file);

#ifdef TARGET_VITA
    vita_bswap_region(pPointerLocation, FenHdr.PointerTableLength, SWAP_U32);
#endif

    m_pFEPackage = (FEPackage*)pData;

    pLastPointer = (unsigned long*)((unsigned char*)pPointerLocation + (FenHdr.PointerTableLength & ~3));
    for (pCurrentPointer = pPointerLocation; pCurrentPointer < pLastPointer; pCurrentPointer++)
    {
        unsigned long offset = *pCurrentPointer;
        pPointer = (unsigned long*)((unsigned char*)pData + offset);
        RelocatePointer(pPointer, pData);
    }

    nlFree(pPointerLocation);

#ifdef TARGET_VITA
    vita_fen_fix_package((FEPackage*)pData);
#endif

    file = (nlFile*)m_pFEPackage;
    QueueResourceLoadCallback cb;

    cb.m_resourceManager = FEResourceManager::Instance();
    m_feSceneResourceHandle.m_pFESceneContext = this;
    m_feSceneResourceHandle.m_hashID = m_uHashID;
    m_feSceneResourceHandle.m_next = 0;
    m_feSceneResourceHandle.m_prev = 0;
    m_feSceneResourceHandle.m_type = FERT_SCENE;

    FEResourceManager::Instance()->QueueResourceLoad(&m_feSceneResourceHandle);
    nlWalkRing<FEResourceHandle, QueueResourceLoadCallback>(((FEPackage*)file)->m_pResourceList, &cb, &QueueResourceLoadCallback::Callback);
    return true;
}

/**
 * Offset/Address/Size: 0x34C | 0x8020A0C0 | size: 0x7C
 */
FEScene::~FEScene()
{
    if (m_pFEPackage != NULL)
    {
        if (gSebringLoadPackageToVirtualMemory)
        {
            nlVirtualFree(m_pFEPackage);
        }
        else
        {
            delete[] m_pFEPackage;
        }
        m_pFEPackage = NULL;
        m_uHashID = 0;
    }
}

/**
 * Offset/Address/Size: 0x3C8 | 0x8020A13C | size: 0x8C
 */
FEScene::FEScene()
    : m_pFEPackage(NULL)
    , m_uHashID(0)
    , m_bValid(false)
    , m_uRenderView(0)
{
    nlVector3 FROM;
    nlVec3Set(FROM, 0.0f, 0.0f, 600.0f);
    nlVector3 TO;
    nlVec3Set(TO, 0.0f, 0.0f, 0.0f);
    nlVector3 UP;
    nlVec3Set(UP, 0.0f, 1.0f, 0.0f);
    glMatrixLookAt(m_matView, FROM, TO, UP);
}
