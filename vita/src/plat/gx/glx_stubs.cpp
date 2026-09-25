// Real matrix and platform GX implementations for VitaGL.

#include "NL/gl/glPlat.h"
#include "NL/gl/glMatrix.h"
#include "NL/glx/glxLoadModel.h"
#include "NL/glx/glxTexture.h"
#include "NL/glx/glxMemory.h"
#include "NL/glx/glxTarget.h"
#include "NL/glx/glxSwap.h"
#include "NL/glx/glxMatrix.h"
#include "NL/glx/glxFont.h"
#include "plat/vita_tex_mgr.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// glxLoadModel.h
bool glplatBeginLoadModel(const char* filename, void (*callback)(void*, unsigned long, void*), void* userData) { return false; }
static char g_DummyModel[1024];
glModel* glplatEndLoadModel(void* data, unsigned long size, unsigned long* pNumModels) { return (glModel*)g_DummyModel; }
glModel* glplatLoadModel(const char* filename, unsigned long* pNumModels) { return (glModel*)g_DummyModel; }
void glSetIgnoreDuplicateModels(bool ignore) {}
static char g_DummySkinMesh[1024];
GLSkinMesh* glx_MakeSkinMesh(nlChunk* outerChunk, glModel* models) { return (GLSkinMesh*)g_DummySkinMesh; }

// glxTexture.h
bool glplatBeginLoadTextureBundle(const char* filename, void (*callback)(void*, unsigned long, void*), void* param) { return false; }
bool glplatEndLoadTextureBundle(void* data, unsigned long size) { return false; }
u32 glplatTextureGetHeight() { return 1; }
int glplatTextureGetNumBits(int component) { return 8; }
u32 glplatTextureGetWidth() { return 1; }
bool glplatTextureLoad(unsigned long texture) { return true; }
static char g_DummyPlatTexture[1024];
PlatTexture* glx_CreatePlatTexture() { return (PlatTexture*)g_DummyPlatTexture; }
bool glx_AddTex(unsigned long handle, PlatTexture* pTex) { return true; }
PlatTexture* glx_GetTex(unsigned long handle, bool bMissingFatal, bool bAllowGrids) { return (PlatTexture*)g_DummyPlatTexture; }
glxTextureLoadCallback_t glx_SetLoadCallback(glxTextureLoadCallback_t callback) { return nullptr; }

// glxFont.h
bool glplatCreateFont(unsigned long width, unsigned long height, const unsigned short* data, unsigned long handle) { return false; }

// glxMemory.h
static uint8_t s_frame_pool[3][2 * 1024 * 1024]; // 2 MB per frame, 3-frame ring
static size_t s_frame_used = 0;
static int s_frame_idx = 0;

void* glplatFrameAlloc(unsigned long size, eGLMemory memType)
{
    (void)memType;
    size = (size + 15) & ~15;
    if (s_frame_used + size <= sizeof(s_frame_pool[0])) {
        void* ptr = &s_frame_pool[s_frame_idx][s_frame_used];
        s_frame_used += size;
        return ptr;
    }
    return std::malloc(size);
}

void glplatFrameAllocNextFrame(void)
{
    s_frame_idx = (s_frame_idx + 1) % 3;
    s_frame_used = 0;
}

void glplatGetMatrix(unsigned long matrix, nlMatrix4& m)
{
    if (matrix != 0 && matrix != 0xFFFFFFFF) {
        m = *(const nlMatrix4*)matrix;
    } else {
        m.SetIdentity();
    }
}

void* glplatResourceAlloc(unsigned long size, eGLMemory memType)
{
    (void)memType;
    return std::malloc(size);
}

unsigned long long glplatResourceMark() { return 0; }
void glplatResourceRelease(unsigned long long marker) { (void)marker; }

void glplatSetMatrix(unsigned long matrix, const nlMatrix4& m)
{
    if (matrix != 0 && matrix != 0xFFFFFFFF) {
        *(nlMatrix4*)matrix = m;
    }
}

// glxTarget.h
void glPlatGrabFrameBufferToTexture(unsigned long texture, unsigned int destWidth, unsigned int destHeight, unsigned int srcLeft, unsigned int srcTop, unsigned int srcWidth, unsigned int srcHeight) {}
bool glx_GetSharedLock() { return false; }
u32 glx_GetSharedMemory() { return 0; }
u32 glx_GetSharedMemorySize() { return 0; }
void glx_LockSharedMemory() {}
void glx_UnlockSharedMemory() {}

// glxMatrix.h
static void C_MTXOrtho(float m[4][4], float t, float b, float l, float r, float n, float f)
{
    float tmp = 1.0f / (r - l);
    m[0][0] = 2.0f * tmp;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = (tmp * -(r + l));

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = 2.0f * tmp;
    m[1][2] = 0.0f;
    m[1][3] = (tmp * -(t + b));

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = (-1.0f * tmp);
    m[2][3] = (-f * tmp);

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

static void C_MTXPerspective(float m[4][4], float fovY, float aspect, float n, float f)
{
    float angle = (0.5f * fovY) * (3.1415926535f / 180.0f);
    float cot = 1.0f / std::tan(angle);

    m[0][0] = cot / aspect;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    float tmp = 1.0f / (f - n);
    m[2][2] = (-n * tmp);
    m[2][3] = (tmp * -(f * n));

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

void glplatMatrixOrthographicCentered(nlMatrix4& matrix, float width, float height, float near, float far)
{
    float half = 0.5f;
    C_MTXOrtho(matrix.e2, height * half, -height * half, -width * half, width * half, near, far);
}

void glplatMatrixOrthographic(nlMatrix4& matrix, float width, float height)
{
    static float fNear = 0.0f;
    static float fFar = 16777215.0f;
    C_MTXOrtho(matrix.e2, 0.f, height, 0.f, width, fNear, fFar);
}

void glplatMatrixPerspective(nlMatrix4& matrix, float fovY, float aspect, float near, float far)
{
    float tanHalfFov = std::tan(0.5f * fovY);
    float atanVal = std::atan((1.0f / aspect) / (1.0f / tanHalfFov));
    C_MTXPerspective(matrix.e2, (2.f * atanVal * 180.0f) / 3.1415927f, aspect, near, far);
}

void glplatMatrixLookAt(nlMatrix4& m, const nlVector3& eye, const nlVector3& at, const nlVector3& up)
{
    float x = eye.x - at.x;
    float y = eye.y - at.y;
    float z = eye.z - at.z;
    float lenSq = x * x + y * y + z * z;
    float inverseLength = (lenSq > 0.0f) ? (1.0f / std::sqrt(lenSq)) : 1.0f;

    float upZ = up.z;
    float upY = up.y;
    float forwardX = inverseLength * x;
    float upX = up.x;
    float forwardY = inverseLength * y;
    float forwardZ = inverseLength * z;

    float negUpX = -upX;
    float upZForwardX = upZ * forwardX;
    float upZForwardY = upZ * forwardY;
    float upYForwardX = upY * forwardX;
    x = upY * forwardZ - upZForwardY;
    y = negUpX * forwardZ + upZForwardX;
    z = upX * forwardY - upYForwardX;

    float lenSqSide = z * z + (x * x + y * y);
    inverseLength = (lenSqSide > 0.0f) ? (1.0f / std::sqrt(lenSqSide)) : 1.0f;

    nlVector3 side;
    side.x = inverseLength * x;
    side.y = inverseLength * y;
    side.z = inverseLength * z;

    nlVector3 view;
    view.x = forwardX;
    view.y = forwardY;
    view.z = forwardZ;

    float negForwardX = -view.x;
    float cameraUpX = (view.y * side.z) - (view.z * side.y);
    nlVector3 cameraUp;
    cameraUp.x = cameraUpX;
    cameraUp.y = (negForwardX * side.z) + (view.z * side.x);
    cameraUp.z = (view.x * side.y) - (view.y * side.x);

    m.SetColumn_(0, side);
    m.e2[3][0] = -(side.x * eye.x + side.y * eye.y + side.z * eye.z);
    m.SetColumn_(1, cameraUp);
    m.e2[3][1] = -(cameraUp.x * eye.x + cameraUp.y * eye.y + cameraUp.z * eye.z);
    m.SetColumn_(2, view);
    m.e2[3][2] = -(view.x * eye.x + view.y * eye.y + view.z * eye.z);
    m.e2[0][3] = 0.0f;
    m.e2[1][3] = 0.0f;
    m.e2[2][3] = 0.0f;
    m.e2[3][3] = 1.0f;
}

// glPlat.h
void glplatViewProjectPoint(eGLView view, const nlVector3& v3world, nlVector3& v3NDC) {}
void glx_ClearXFB(void* cache) {}
u32 glx_GetResetCode() { return 0; }
u32 glx_GetScaledXFBWidth() { return 0; }
u32 glx_GetTargetFPS() { return 0; }
void glx_SetFog(int type) {}
void glx_SetInterlacedMode() {}
void glx_SetPal50Mode() {}
void glx_SetProgressiveMode() {}
void glx_SetRGB60Mode() {}

// glMatrix.h
void glSetMatrix(unsigned int matrix, const nlMatrix4& m)
{
    glplatSetMatrix((unsigned long)matrix, m);
}

// glxSwap.h
void* glxGetBackBuffer() { return nullptr; }
void* glxGetDisplayedBuffer() { return nullptr; }
void glxSwapLoading(bool bBegin, bool bOtherPosition) {}
void glxSwapSetBlack(bool black) {}
