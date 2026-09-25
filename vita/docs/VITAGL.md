# vitaGL: how we should build and use it

Upstream: https://github.com/Rinnegatamante/vitaGL (cloned at /private/tmp/vitaGL-upstream)

## What we link today

The **stock VitaSDK `libvitaGL.a`**, with whatever flags it was packaged with,
unpatched. That is fine for the demo shell but is not what shipping Vita ports
use.

## Build flags: what real ports actually use

Two independent ports converge on a common core.

ACGC (Animal Crossing GC → Vita), `vita/rebuild_vitagl.sh`:
```
BUFFERS_SPEEDHACK=1 DRAW_SPEEDHACK=1 SAMPLERS_SPEEDHACK=1 PRIMITIVES_SPEEDHACK=1
TEXTURES_SPEEDHACK=1 HAVE_SHADER_CACHE=1 NO_DEBUG=1 DRAW_STATE_CACHE=1
PHYCONT_ON_DEMAND=1
```

LDhewm3 ReARMed (Doom 3 → Vita), README:
```
HAVE_WRAPPED_ALLOCATORS=1 NO_DEBUG=1 DRAW_SPEEDHACK=1 CIRCULAR_POOL_SPEEDHACK=1
HAVE_SHADER_CACHE=1 DEPTH_STENCIL_HACK=1
```

**Consensus core:** `NO_DEBUG=1 DRAW_SPEEDHACK=1 HAVE_SHADER_CACHE=1`.

Per-flag notes worth knowing before enabling anything (upstream README plus
ACGC's own comments):

| flag | effect / caveat |
|---|---|
| `HAVE_SHADER_CACHE=1` | caches compiled shaders to disk — Vita3K was already seen doing `shader_cache/v27/*.gxp` lookups |
| `NO_DEBUG=1` | drops error handling; take it last, after things work |
| `DRAW_SPEEDHACK=1` | faster draw calls, "may cause crashes" |
| `TEXTURES_SPEEDHACK=1` | ACGC calls it **required**: `fast_draw_mode` skips `tex->last_frame`, so immediate frees would race in-flight GPU reads. Incompatible with `HAVE_TEXTURE_CACHE=1` |
| `USE_SCRATCH_MEMORY` | ACGC: keep **off**, the pool wrap corrupts |
| `HAVE_VITA3K_SUPPORT=1` | sets `DISABLE_HW_ETC1` — Vita3K does not emulate hardware ETC1. Relevant to us because we develop on the emulator; neither reference port uses it |
| `PRIMITIVES_SPEEDHACK=1` | `GL_LINES` / `GL_POINTS` may glitch |

## A correctness patch, not an optimisation

ACGC patches `source/shaders/glsl_translator_hdr.h`:

```
vglMul mat*mat:  return M1 * M2   ->   return M2 * M1
```

psp2cgc compiles Cg row-major, so the translated GLSL product comes out
transposed. Any matrix maths we push through vitaGL's translator hits this.

## Blocker if we build upstream today

`make` on upstream HEAD fails: `source/vgl.c` calls
`shark_set_shader_association_path()`, which the VitaSDK's installed
`vitashark.h` does not declare (it has `shark_init` and `shark_set_allocators`
only). **Updating vitashark is a prerequisite** for building current vitaGL from
source.

## How we should use it

ACGC's `vita/src/vita_gx_cmdbuf.c` (100 KB) is the model: translate GX to
**ordinary GL calls**, dropping to vitaGL extensions only for zero-copy paths
(`vglBufferData`, `vglMalloc`, `vglFree`, `vglGetGxmTextureById`,
`vglGetGxmContext`) and to `sceGxmSetFragmentTexture` where GL cannot express
it. We do not need to reimplement sceGxm.

Two constraints already established in this port, from `_vita_status.txt`
tracing and Vita3K logs:
- Vita3K's Vulkan renderer does **not** implement the GXM mask, so a scissored
  `glClear` wipes the whole framebuffer. Draw real geometry instead.
- vitaGL's fixed-function path needs **three-component** positions;
  `glVertexPointer(2, ...)` draws nothing.
- Keep `VITA_ENABLE_GXP=0`: precompiled-GXP via `glShaderBinary` /
  `glLinkProgram` crashes the Vita3K **host** process.

## Recommended order when we reach rendering

Do not swap vitaGL while endianness is still the blocker. When we do:

1. Update vitashark, then build upstream vitaGL unchanged, install, confirm the
   demo VPK (`VSTR00002`) still renders its menu. Baseline first.
2. Add the consensus core: `NO_DEBUG=1 DRAW_SPEEDHACK=1 HAVE_SHADER_CACHE=1`.
   Re-verify.
3. Add `HAVE_VITA3K_SUPPORT=1`. Re-verify.
4. Apply the `vglMul` transpose patch before trusting any matrix result.
5. Only then consider the remaining speedhacks, one at a time.

An earlier attempt in this port to switch to the ACGC vitaGL fork regressed even
clear-only rendering on Vita3K. One variable at a time, verify each.
