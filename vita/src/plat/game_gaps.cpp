// Game-side definitions the decomp does not yet contain.
//
// These are not platform code: the GameCube build gets them from object files
// that have not been decompiled yet, so the Vita link has nothing to bind to.
// Each one is a placeholder with the real signature, so the link closes and the
// game can boot; replace them as the decomp fills in.

#include "types.h"

#include "Game/AI/FuzzyDebugger.h"
#include "Game/AI/StatsGatherer.h"
// Replay.h first: LoadFrame.h depends on ReplayablePod from it, and with
// -fpermissive a wrong order silently mis-declares RenderSnapshot::Replay.
#include "Game/Replay.h"
#include "Game/LoadFrame.h"
#include "Game/RenderSnapshot.h"
#include "Game/World/worldanim.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// StatsGatherer — the AI batch-testing harness. Never runs in a normal match.
// ---------------------------------------------------------------------------

void StatsGatherer::Run(float dt)
{
    (void)dt;
}

const char* StatsGatherer::GetName()
{
    return "StatsGatherer";
}

void StatsGatherer::DoFunctionCall(unsigned int function)
{
    (void)function;
}

// ---------------------------------------------------------------------------
// cFuzzyDebugger — AI decision-tree tracing, debug builds only.
// ---------------------------------------------------------------------------

void cFuzzyDebugger::WriteXML()
{
}

void cFuzzyDebugger::ClearAllTrees()
{
}

// ---------------------------------------------------------------------------
// World animation controllers
// ---------------------------------------------------------------------------

TMAnimController::TMAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext)
    : WorldAnimController(szAnimSetAndHierarchyName, pWorldContext)
    , m_pRootModel(nullptr)
    , m_uLastFrameUpdated(0)
    , m_pWorldContext(pWorldContext)
{
}

// The virtual destructor is each class's key function: without it GCC emits no
// vtable at all.
TMAnimController::~TMAnimController()
{
}

void TMAnimController::Update(float fTimeDelta)
{
    (void)fTimeDelta;
}

glModel* TMAnimController::GetUpdatedModel()
{
    return nullptr;
}

SkinnedAnimController::SkinnedAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext)
    : WorldAnimController(szAnimSetAndHierarchyName, pWorldContext)
    , m_pSkinMesh(nullptr)
    , m_pCachedSkinnedModel(nullptr)
    , m_pSkinModel(nullptr)
{
}

SkinnedAnimController::~SkinnedAnimController()
{
}

void SkinnedAnimController::Update(float fTimeDelta)
{
    (void)fTimeDelta;
}

void SkinnedAnimController::UpdateSkinnedMesh(unsigned long program, void* pLightData)
{
    (void)program;
    (void)pLightData;
}

void SkinnedAnimController::CreateGLSkinMesh(glModel* pModel)
{
    (void)pModel;
}

// ---------------------------------------------------------------------------
// RenderSnapshot::Replay is a template defined in the header; Metrowerks
// instantiated it for the two frame types the replay system uses.
// ---------------------------------------------------------------------------

// An explicit instantiation emits nothing here, so force the two the replay
// system needs by referencing them. Never called.
// KNOWN GAP: RenderSnapshot::Replay is a template defined in RenderSnapshot.h,
// but GCC will not instantiate it here — it emits an undefined reference even
// with the definition in the translation unit, and neither an explicit
// instantiation nor a forcing call changes that. Metrowerks evidently emitted
// it from a translation unit we have not identified.
//
// These specialisations close the link so the game can boot. They make replay
// recording and playback do nothing; matches are unaffected. Replace them once
// the instantiation problem is understood.
template <>
void RenderSnapshot::Replay<LoadFrame>(LoadFrame& frame)
{
    (void)frame;
}

template <>
void RenderSnapshot::Replay<SaveFrame>(SaveFrame& frame)
{
    (void)frame;
}
