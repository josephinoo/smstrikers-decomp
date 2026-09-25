/* Pure-C platvmath for Vita (NL/plat/platvmath.cpp is PPC asm). */
#include "NL/nlMath.h"
#include "NL/platvmath.h"

#include <cmath>

void nlMatrix4::SetIdentity()
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            e2[r][c] = (r == c) ? 1.0f : 0.0f;
        }
    }
}

void nlMultMatrices(nlMatrix4& out, const nlMatrix4& a, const nlMatrix4& b)
{
    nlMatrix4 temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp.e2[i][j] = a.e2[i][0] * b.e2[0][j] + a.e2[i][1] * b.e2[1][j]
                + a.e2[i][2] * b.e2[2][j] + a.e2[i][3] * b.e2[3][j];
        }
    }
    out = temp;
}

void nlTransposeMatrix(nlMatrix4& out, const nlMatrix4& in)
{
    nlMatrix4 temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp.e2[i][j] = in.e2[j][i];
        }
    }
    out = temp;
}

void nlInvertMatrix(nlMatrix4& out, const nlMatrix4& in)
{
    /* ponytail: identity fallback until a real 4x4 inverse is needed for gameplay. */
    (void)in;
    out.SetIdentity();
}

void nlMultVectorMatrix(nlVector2& v_out, const nlVector2& v_in, const nlMatrix3& m)
{
    nlVector2 t;
    t.x = m.e[0] * v_in.x + m.e[3] * v_in.y + m.e[6];
    t.y = m.e[1] * v_in.x + m.e[4] * v_in.y + m.e[7];
    v_out = t;
}

void nlMultPosVectorMatrix(nlVector3& result, const nlVector3& pos, const nlMatrix4& m)
{
    result.x = m.e2[0][0] * pos.x + m.e2[1][0] * pos.y + m.e2[2][0] * pos.z + m.e2[3][0];
    result.y = m.e2[0][1] * pos.x + m.e2[1][1] * pos.y + m.e2[2][1] * pos.z + m.e2[3][1];
    result.z = m.e2[0][2] * pos.x + m.e2[1][2] * pos.y + m.e2[2][2] * pos.z + m.e2[3][2];
}

void nlMultVectorMatrix(nlVector4& out, const nlVector4& in, const nlMatrix4& m)
{
    nlVector4 temp;
    temp.x = m.e2[0][0] * in.x + m.e2[1][0] * in.y + m.e2[2][0] * in.z + m.e2[3][0] * in.w;
    temp.y = m.e2[0][1] * in.x + m.e2[1][1] * in.y + m.e2[2][1] * in.z + m.e2[3][1] * in.w;
    temp.z = m.e2[0][2] * in.x + m.e2[1][2] * in.y + m.e2[2][2] * in.z + m.e2[3][2] * in.w;
    temp.w = m.e2[0][3] * in.x + m.e2[1][3] * in.y + m.e2[2][3] * in.z + m.e2[3][3] * in.w;
    out = temp;
}

void nlMultDirVectorMatrix(nlVector3& result, const nlVector3& direction, const nlMatrix4& m)
{
    result.x = m.e2[0][0] * direction.x + m.e2[1][0] * direction.y + m.e2[2][0] * direction.z;
    result.y = m.e2[0][1] * direction.x + m.e2[1][1] * direction.y + m.e2[2][1] * direction.z;
    result.z = m.e2[0][2] * direction.x + m.e2[1][2] * direction.y + m.e2[2][2] * direction.z;
}

void nlMakeRotationMatrixX(nlMatrix4& out, float theta)
{
    const float cs = std::cos(theta);
    const float sn = std::sin(theta);
    out.SetIdentity();
    out.e2[1][1] = cs;
    out.e2[1][2] = sn;
    out.e2[2][1] = -sn;
    out.e2[2][2] = cs;
}

void nlMakeRotationMatrixY(nlMatrix4& out, float theta)
{
    const float cs = std::cos(theta);
    const float sn = std::sin(theta);
    out.SetIdentity();
    out.e2[0][0] = cs;
    out.e2[0][2] = -sn;
    out.e2[2][0] = sn;
    out.e2[2][2] = cs;
}

void nlMakeRotationMatrixZ(nlMatrix4& out, float theta)
{
    const float cs = std::cos(theta);
    const float sn = std::sin(theta);
    out.SetIdentity();
    out.e2[0][0] = cs;
    out.e2[0][1] = sn;
    out.e2[1][0] = -sn;
    out.e2[1][1] = cs;
}

void nlMakeRotationMatrixZ(nlMatrix3& out, float theta)
{
    const float cs = std::cos(theta);
    const float sn = std::sin(theta);
    for (int i = 0; i < 9; i++) {
        out.e[i] = 0.0f;
    }
    out.e[0] = cs;
    out.e[1] = sn;
    out.e[3] = -sn;
    out.e[4] = cs;
    out.e[8] = 1.0f;
}

void nlMakeRotationMatrixEulerAngles(nlMatrix4& m, float pitch, float yaw, float roll)
{
    nlMatrix4 rx, ry, rz, t;
    nlMakeRotationMatrixX(rx, pitch);
    nlMakeRotationMatrixY(ry, yaw);
    nlMakeRotationMatrixZ(rz, roll);
    nlMultMatrices(t, ry, rx);
    nlMultMatrices(m, rz, t);
}

void nlMakeScaleMatrix(nlMatrix4& m, float sx, float sy, float sz)
{
    m.SetIdentity();
    m.e2[0][0] = sx;
    m.e2[1][1] = sy;
    m.e2[2][2] = sz;
}
