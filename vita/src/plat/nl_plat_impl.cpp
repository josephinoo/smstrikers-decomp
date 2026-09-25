// NL platform pieces the Vita port owns: the assert hook, the GX display-list
// façade (the Vita renderer does not use display lists), and the LexicalCast
// instantiations Metrowerks emitted implicitly.
//
// The pad, quaternion math and texture swizzler are NOT here: those decomp
// sources are portable and are compiled directly (see vita/CMakeLists.txt).

#include "types.h"

#include "NL/nlDebug.h"
#include "NL/nlBasicString.h"
#include "NL/nlLexicalCast.h"
#include "NL/glx/glxDisplayList.h"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Asserts
// ---------------------------------------------------------------------------

void nlAssertFail(const char* condition, const char* filename, int line, bool bBreak)
{
    std::printf("[ASSERT] %s\n  at %s:%d\n", condition, filename, line);
    if (bBreak) {
        std::abort();
    }
}

// ---------------------------------------------------------------------------
// LexicalCast
//
// The primary template only declares Do(); Metrowerks emitted a definition per
// instantiation. Provide one and instantiate the cases the game links against.
// ---------------------------------------------------------------------------

namespace Detail
{

template <typename To, typename From>
To LexicalCastImpl<To, From>::Do(const From& f)
{
    // Every instantiation the game links against casts a character array into a
    // BasicString, whose const CharT* constructor takes the decayed array.
    return To(f);
}

// The instantiations Metrowerks emitted implicitly.
typedef BasicString<char, TempStringAllocator> TempString;
typedef BasicString<unsigned short, TempStringAllocator> TempWString;

template struct LexicalCastImpl<TempString, char[5]>;
template struct LexicalCastImpl<TempString, char[64]>;
template struct LexicalCastImpl<TempWString, unsigned short[2]>;
template struct LexicalCastImpl<TempWString, unsigned short[4]>;
template struct LexicalCastImpl<TempWString, unsigned short[8]>;
template struct LexicalCastImpl<TempWString, unsigned short[16]>;
template struct LexicalCastImpl<TempWString, unsigned short[32]>;
template struct LexicalCastImpl<TempWString, unsigned short[128]>;

} // namespace Detail

// ---------------------------------------------------------------------------
// Dolphin quaternion helpers
//
// src/NL/plat/platqmath.cpp is portable and compiles for the Vita, but it calls
// into the paired-single PPC maths library for these three. Plain C++ here.
// ---------------------------------------------------------------------------

#include "dolphin/mtx.h"

#include <cmath>

void PSQUATScale(const Quaternion* q, Quaternion* r, f32 scale)
{
    r->x = q->x * scale;
    r->y = q->y * scale;
    r->z = q->z * scale;
    r->w = q->w * scale;
}

f32 PSQUATDotProduct(const Quaternion* p, const Quaternion* q)
{
    return p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
}

void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)
{
    f32 dot = PSQUATDotProduct(p, q);

    // Take the shorter arc: q and -q are the same rotation.
    f32 sign = 1.0f;
    if (dot < 0.0f) {
        dot = -dot;
        sign = -1.0f;
    }

    f32 wp, wq;
    if (dot > 0.99999f) {
        // Nearly parallel: lerp, or the sine below goes to zero.
        wp = 1.0f - t;
        wq = t;
    } else {
        const f32 theta = std::acos(dot);
        const f32 inv_sin = 1.0f / std::sin(theta);
        wp = std::sin((1.0f - t) * theta) * inv_sin;
        wq = std::sin(t * theta) * inv_sin;
    }
    wq *= sign;

    r->x = p->x * wp + q->x * wq;
    r->y = p->y * wp + q->y * wq;
    r->z = p->z * wp + q->z * wq;
    r->w = p->w * wp + q->w * wq;
}

// ---------------------------------------------------------------------------
// PlatTexture
//
// On the GameCube this wrapped a GXTexObj over swizzled tile data. The Vita
// keeps the linear bytes and hands them to vitaGL when the texture is actually
// bound, so Prepare/Swizzle have nothing to do here.
// ---------------------------------------------------------------------------

#include "NL/glx/glxTexture.h"
#include "NL/gc/gcSwizzler.h"

#include <cstring>

void PlatTexture::Create(int width, int height, eGXTextureFormat format,
                         int numLevels, bool bLinearData, bool bNewResourceMemory)
{
    (void)bNewResourceMemory;

    m_Width = static_cast<u16>(width);
    m_Height = static_cast<u16>(height);
    m_Levels = static_cast<u8>(numLevels);
    m_MaxLevel = static_cast<u8>(numLevels > 0 ? numLevels - 1 : 0);
    m_Format = format;

    const u32 bytes = GCTextureSize(format, width, height, numLevels, 0);
    void* storage = bytes != 0 ? std::calloc(bytes, 1) : nullptr;
    if (bLinearData) {
        m_LinearData = storage;
    } else {
        m_SwizzledData = storage;
    }
}

void PlatTexture::CreateWithMemory(int width, int height, eGXTextureFormat format,
                                   int numLevels, const void* pTextureData)
{
    m_Width = static_cast<u16>(width);
    m_Height = static_cast<u16>(height);
    m_Levels = static_cast<u8>(numLevels);
    m_MaxLevel = static_cast<u8>(numLevels > 0 ? numLevels - 1 : 0);
    m_Format = format;
    // Borrowed, not owned: this comes out of a mapped texture bundle.
    m_SwizzledData = const_cast<void*>(pTextureData);
}

void PlatTexture::Prepare()
{
    // GX flushed the texture cache here. vitaGL uploads on bind instead.
}

void PlatTexture::Swizzle(bool bDeleteLinear)
{
    if (m_LinearData == nullptr || m_SwizzledData != nullptr) {
        return;
    }
    const u32 bytes = GCTextureSize(m_Format, m_Width, m_Height, m_Levels, 0);
    if (bytes == 0) {
        return;
    }
    m_SwizzledData = std::calloc(bytes, 1);
    if (m_SwizzledData == nullptr) {
        return;
    }
    GCSwizzle(m_SwizzledData, m_LinearData, m_Width, m_Height, m_Format, false);
    if (bDeleteLinear) {
        std::free(m_LinearData);
        m_LinearData = nullptr;
    }
}
