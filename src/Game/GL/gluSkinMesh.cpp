#include "Game/GL/gluSkinMesh.h"
#include "types.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glState.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/nlDLRing.h"
#include "NL/nlString.h"
#include "NL/platvmath.h"
#include "dolphin/PPCArch.h"
#include "dolphin/os/OSCache.h"

#define qr0 0

class TempMatrixCopier
{
public:
    /**
     * Offset/Address/Size: 0x4F0 | 0x801B6034 | size: 0x60
     */
    void CopyMatrix(const unsigned long& boneId, unsigned long* outValue)
    {
        SkinMatrix& matrix = (SkinMatrix&)m_Mesh->GetPoseMatrix(boneId);
        matrix.Get(m_TempMatrices[*outValue]);
    }

    /* 0x00 */ nlMatrix4* m_TempMatrices;
    /* 0x04 */ ShaderSkinMesh* m_Mesh;
}; // total size: 0x8

static inline int SkinIndexOf(const SkinPair& p)
{
    return p.vertexIndex;
}

/**
 * Offset/Address/Size: 0x550 | 0x801B6094 | size: 0xDC
 */
void ShaderSkinMesh::StitchModel()
{
    int packetIndex = 0;
    glModelPacket* pPacket = pModel->packets;
    for (; pPacket < pModel->packets + pModel->numPackets; packetIndex++, pPacket++)
    {
        if (glGetRasterState(pPacket->state.raster, GLS_SolidOffset) != 1)
            continue;
        DisplayList* dl = dlGetStruct(pPacket->indexBuffer);
        u8* pWrite = (u8*)dl->list;
        if (*(pWrite += 3) != 0xff)
            continue;
        for (int i = 0; i < pPacket->numVertices; i++)
        {
            *pWrite = (stitchArray[packetIndex][i] + 1) * 3;
            pWrite += (pPacket->numStreams - 1) * 2 + 1;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801B5B44 | size: 0x4F0
 */
void ShaderSkinMesh::AttachSkinData(unsigned long program, const nlMatrix4* pReflect)
{
    nlVector3* outVertices = NULL;
    nlVector3* outNormals = NULL;
    nlAVLTree<unsigned long, unsigned long, DefaultKeyCompare<unsigned long> >* boneMap = &nlRingGetStart<BoneMapList>(boneMaps)->boneMap;

    if (boneMap->m_NumElements != 0)
    {
        if (tempMatrices == NULL)
        {
            tempMatrices = (nlMatrix4*)nlMalloc(boneMap->m_NumElements * sizeof(nlMatrix4), 8, false);
        }

        outVertices = (nlVector3*)glFrameAlloc(numSoftwareVerts * sizeof(nlVector3), GLM_VertexData);
        outNormals = (nlVector3*)glFrameAlloc(numSoftwareVerts * sizeof(nlVector3), GLM_VertexData);

        nlZeroMemory(outVertices, numSoftwareVerts * sizeof(nlVector3));
        nlZeroMemory(outNormals, numSoftwareVerts * sizeof(nlVector3));

        float vertexWeight;

        TempMatrixCopier matCopier;
        matCopier.m_TempMatrices = tempMatrices;
        matCopier.m_Mesh = this;

        boneMap->Walk(&matCopier, &TempMatrixCopier::CopyMatrix);

        SkinPairList* curr = nlRingGetStart<SkinPairList>(skinPairs);
        int matrixOffset = 0;

        if (curr != NULL)
        {
            while (true)
            {
                if (curr->num != 0) {
#ifdef __MWERKS__
                    // clang-format off
                    register const nlMatrix4* pMatrix = &tempMatrices[matrixOffset];
                    asm {
                        psq_l f2, 0x0(pMatrix), 0, qr0
                        psq_l f3, 0x8(pMatrix), 0, qr0
                        psq_l f4, 0x10(pMatrix), 0, qr0
                        psq_l f5, 0x18(pMatrix), 0, qr0
                        psq_l f6, 0x20(pMatrix), 0, qr0
                        psq_l f7, 0x28(pMatrix), 0, qr0
                        psq_l f8, 0x30(pMatrix), 0, qr0
                        psq_l f9, 0x38(pMatrix), 0, qr0
                    }
                    // clang-format on
#endif
                }

                for (int i = 0; i < (int)curr->num; i++)
                {
                    const SkinPair& pair = curr->pairs[i];
                    vertexWeight = (float)pair.vertexWeight / 65535.0f;
                    int index = SkinIndexOf(pair);

                    const nlVector3& inVertex = (morphBuffer != NULL) ? morphBuffer[index] : softwareVertices[index].position;

                    const signed char* packed = softwareVertices[index].packed_normal;
                    float invNormalScale = 0.015625f;
                    nlVector3 inNormal;
                    inNormal.x = (float)packed[0] * invNormalScale;
                    inNormal.y = (float)packed[1] * invNormalScale;
                    inNormal.z = (float)packed[2] * invNormalScale;

                    nlVector3& outVertex = outVertices[index];
                    nlVector3& outNormal = outNormals[index];

#ifdef __MWERKS__
                    register const nlVector3* pInN = &inNormal;
                    // clang-format off
                    asm {
                        psq_l f17, 0x0(inVertex), 0, qr0
                        psq_l f18, 0x8(inVertex), 1, qr0
                        ps_muls0 f15, f2, f17
                        ps_muls0 f16, f3, f17
                        ps_madds1 f15, f4, f17, f15
                        ps_madds1 f16, f5, f17, f16
                        ps_madds0 f15, f6, f18, f15
                        ps_madds0 f16, f7, f18, f16
                        ps_add f15, f8, f15
                        ps_add f16, f9, f16
                        psq_l f0, 0x0(pInN), 0, qr0
                        psq_l f1, 0x8(pInN), 1, qr0
                        ps_muls0 f10, f2, f0
                        ps_muls0 f11, f3, f0
                        ps_madds1 f10, f4, f0, f10
                        ps_madds1 f11, f5, f0, f11
                        ps_madds0 f10, f6, f1, f10
                        ps_madds0 f11, f7, f1, f11
                        lfs f12, vertexWeight
                        psq_l f19, 0x0(outVertex), 0, qr0
                        psq_l f20, 0x8(outVertex), 1, qr0
                        psq_l f13, 0x0(outNormal), 0, qr0
                        psq_l f14, 0x8(outNormal), 1, qr0
                        ps_madds0 f15, f15, f12, f19
                        ps_madds0 f16, f16, f12, f20
                        ps_madds0 f10, f10, f12, f13
                        ps_madds0 f11, f11, f12, f14
                        psq_st f10, 0x0(outNormal), 0, qr0
                        psq_st f11, 0x8(outNormal), 1, qr0
                        psq_st f15, 0x0(outVertex), 0, qr0
                        psq_st f16, 0x8(outVertex), 1, qr0
                    }
                    // clang-format on
#else
                    const nlMatrix4& M = tempMatrices[matrixOffset];
                    nlVector3 tv, tn;
                    nlMultPosVectorMatrix(tv, inVertex, M);
                    nlMultDirVectorMatrix(tn, inNormal, M);
                    outVertex.x += tv.x * vertexWeight;
                    outVertex.y += tv.y * vertexWeight;
                    outVertex.z += tv.z * vertexWeight;
                    outNormal.x += tn.x * vertexWeight;
                    outNormal.y += tn.y * vertexWeight;
                    outNormal.z += tn.z * vertexWeight;
#endif
                }

                if (nlRingIsEnd<SkinPairList>(skinPairs, curr))
                {
                    break;
                }

                curr = curr->m_next;
                matrixOffset++;
            }
        }

        DCFlushRangeNoSync(outVertices, numSoftwareVerts * sizeof(nlVector3));
        DCFlushRangeNoSync(outNormals, numSoftwareVerts * sizeof(nlVector3));
        PPCSync();
    }

    unsigned long matrix;
    if (pReflect == NULL)
    {
        matrix = glGetIdentityMatrix();
    }
    else
    {
        matrix = glAllocMatrix();
        if (matrix != 0xFFFFFFFF)
        {
            glSetMatrix(matrix, *pReflect);
        }
    }

    BoneMapList* mapList = nlRingGetStart<BoneMapList>(boneMaps)->m_next;
    glModelPacket* pPacket = pModel->packets;

    while (pPacket < pModel->packets + pModel->numPackets)
    {
        pPacket->state.matrix = matrix;

        if (program != 0xFFFFFFFF)
        {
            pPacket->state.program = program;
        }

        if (glGetRasterState(pPacket->state.raster, GLS_SolidOffset) == 1)
        {
            glUserAttach(MakeUserData(&mapList->boneMap), pPacket, false);
        }
        else
        {
            pPacket->streams[0].address = (u32)outVertices;
            pPacket->streams[1].address = (u32)outNormals;
            pPacket->streams[1].stride = 0xC;
        }

        mapList = mapList->m_next;
        pPacket++;
    }
}
