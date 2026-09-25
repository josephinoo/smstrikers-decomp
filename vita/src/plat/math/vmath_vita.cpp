#include "plat_abi.h"

#include <cstring>

void vita_mat4_identity(VitaMat4* out)
{
    if (out == nullptr) {
        return;
    }
    std::memset(out->m, 0, sizeof(out->m));
    out->m[0] = out->m[5] = out->m[10] = out->m[15] = 1.0f;
}

void vita_mat4_scale(VitaMat4* out, float sx, float sy, float sz)
{
    vita_mat4_identity(out);
    if (out == nullptr) {
        return;
    }
    out->m[0] = sx;
    out->m[5] = sy;
    out->m[10] = sz;
}

void vita_mat4_mul_pos(float out[3], const float pos[3], const VitaMat4* m)
{
    if (out == nullptr || pos == nullptr || m == nullptr) {
        return;
    }
    const float* a = m->m;
    out[0] = a[0] * pos[0] + a[4] * pos[1] + a[8] * pos[2] + a[12];
    out[1] = a[1] * pos[0] + a[5] * pos[1] + a[9] * pos[2] + a[13];
    out[2] = a[2] * pos[0] + a[6] * pos[1] + a[10] * pos[2] + a[14];
}
