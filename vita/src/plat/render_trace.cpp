// TEMP diagnostics: link-time --wrap counters on the FE/gl submission path.
#include <cstdio>

extern "C" {
int __real__ZN8FERender11RenderSceneEP7FEScene(void* scene);
int __real__ZN8FERender18RenderPresentationEPK14FEPresentation(void* p);
int __real__ZN8FERender19RenderImageInstanceEPK15TLImageInstance(void* self, void* inst);
void __real__Z17glViewAttachModel7eGLViewmPK7glModel(int view, unsigned long arg, const void* model);
bool __real__ZN12GLRenderList11AttachModelEPK7glModelm(void* self, const void* model, unsigned long layer);

int g_rt_scene, g_rt_pres, g_rt_image, g_rt_attach, g_rt_rl_attach, g_rt_rl_fail;
int g_rt_last_view = -1;

int __wrap__ZN8FERender11RenderSceneEP7FEScene(void* scene)
{
    ++g_rt_scene;
    return __real__ZN8FERender11RenderSceneEP7FEScene(scene);
}
int __wrap__ZN8FERender18RenderPresentationEPK14FEPresentation(void* p)
{
    ++g_rt_pres;
    return __real__ZN8FERender18RenderPresentationEPK14FEPresentation(p);
}
int __wrap__ZN8FERender19RenderImageInstanceEPK15TLImageInstance(void* self, void* inst)
{
    ++g_rt_image;
    return __real__ZN8FERender19RenderImageInstanceEPK15TLImageInstance(self, inst);
}
void __wrap__Z17glViewAttachModel7eGLViewmPK7glModel(int view, unsigned long arg, const void* model)
{
    ++g_rt_attach;
    g_rt_last_view = view;
    __real__Z17glViewAttachModel7eGLViewmPK7glModel(view, arg, model);
}
bool __wrap__ZN12GLRenderList11AttachModelEPK7glModelm(void* self, const void* model, unsigned long layer)
{
    ++g_rt_rl_attach;
    bool ok = __real__ZN12GLRenderList11AttachModelEPK7glModelm(self, model, layer);
    if (!ok) ++g_rt_rl_fail;
    return ok;
}
}

// Walk the presentation the FE is about to render and report why nothing draws.
#define private public
#define protected public
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlInstance.h"
#undef private
#undef protected

extern "C" void vita_trace_log(const char* fmt, ...);

static void rt_walk(TLInstance* head, float t, int depth, int* n)
{
    if (!head || depth > 6) return;
    TLInstance* curr = head->m_next;
    for (int guard = 0; guard < 64 && curr; ++guard) {
        if (*n < 40) {
            vita_trace_log("    %*sinst=%p type=%d valid=%d visible=%d children=%p\n", depth * 2, "",
                           curr, (int)curr->GetType(), (int)curr->IsValidAtTime(t), (int)curr->IsVisible(),
                           curr->pChildren);
        }
        ++*n;
        rt_walk(curr->pChildren, t, depth + 1, n);
        if (curr == head) break;
        curr = curr->m_next;
    }
}

extern "C" FEPresentation* __real__ZNK9FEPackage15GetPresentationEv(const void* self);
extern "C" FEPresentation* __wrap__ZNK9FEPackage15GetPresentationEv(const void* self)
{
    FEPresentation* p = __real__ZNK9FEPackage15GetPresentationEv(self);
    static int calls = 0;
    if (calls++ % 300 == 0) {
        vita_trace_log("[PRES] call=%d pres=%p slides=%p cur=%p\n", calls, p, p ? p->m_slides : nullptr,
                       p ? p->m_currentSlide : nullptr);
        if (p && p->m_currentSlide) {
            TLSlide* s = p->m_currentSlide;
            float t = s->GetCurrentTime();
            vita_trace_log("  slide=%p time=%f instances=%p\n", s, t, s->m_instances);
            int n = 0;
            rt_walk(s->m_instances, t, 0, &n);
            vita_trace_log("  total instances=%d\n", n);
        }
    }
    return p;
}
