# What the ACGC Vita port does, and what we should copy

Source: https://github.com/Brendonm17/ACGC-Vita-Port tree `vita`
(Animal Crossing GameCube → Vita — same problem class as this port.)

## 1. Endianness: swap classes, not field-by-field

ACGC does **not** hand-swap each struct field. It has a small vocabulary of
swap classes and applies one to each data region at load time
(`pc/src/pc_assets.c`, `pc/include/pc_bswap.h`):

```c
enum { SWAP_NONE = 0, SWAP_U16 = 1, SWAP_VTX = 2, SWAP_U32 = 3 };

static void do_swap(void* data, unsigned int size, int type) {
    switch (type) {
        case SWAP_U16: pc_bswap_asset_u16(data, size); break;  /* whole region as u16 */
        case SWAP_VTX: pc_bswap_asset_vtx(data, size); break;  /* 16-byte stride, first 12 bytes as u16 pairs */
        case SWAP_U32: pc_bswap_asset_u32(data, size); break;  /* whole region as u32 */
        default: break;
    }
}
```

`pc_assets.c` is 30k lines and **auto-generated**: a table mapping every asset to
its path, offset, size and swap class. That scale is because ACGC's assets are
baked into the DOL/REL. Ours live in separate files behind `nlBundleFile` and the
`.glt` / `.Res` loaders, so we do not need the giant table — but we should copy
the *shape*:

- One header of primitives (`bswap16/32/64` plus array helpers), not scattered
  `__builtin_bswap32` calls.
- Per-region swap classes chosen by the loader from the format's own structure.
- Swap **once**, at load, in the loader. Never in consuming code.

Note `pc_assets_pal_n64_to_gc()`: they also convert pixel formats on load, not
just byte order. Expect the same for our GC texture formats.

## 2. They use vitaGL, not raw sceGxm

`vita/src/vita_gx_cmdbuf.c` (100 KB) translates GX command buffers to GL. It is
ordinary GL calls plus a handful of vitaGL extensions for zero-copy paths:
`vglBufferData`, `vglMalloc`, `vglFree`, `vglGetGxmTextureById`,
`vglGetGxmContext`, dropping to `sceGxmSetFragmentTexture` only where GL cannot
express it. So a GX→vitaGL bridge is the proven approach; we do not need to
reimplement sceGxm.

## 3. vitaGL build flags are load-bearing

`vita/rebuild_vitagl.sh` builds a fork (`Brendonm17/vitaGL`, branch
`async-compressed-tex-prep`) with:

```
BUFFERS_SPEEDHACK=1 DRAW_SPEEDHACK=1 SAMPLERS_SPEEDHACK=1 PRIMITIVES_SPEEDHACK=1
TEXTURES_SPEEDHACK=1 HAVE_SHADER_CACHE=1 NO_DEBUG=1 DRAW_STATE_CACHE=1
PHYCONT_ON_DEMAND=1
```

With two hard-won notes in their own comments:
- `TEXTURES_SPEEDHACK` is **required**: `fast_draw_mode` skips `tex->last_frame`,
  so immediate frees would race in-flight GPU reads.
- `USE_SCRATCH_MEMORY` must stay **off**: the pool wrap corrupts.

## 4. A correctness patch stock vitaGL needs

They patch `source/shaders/glsl_translator_hdr.h`:

```
vglMul mat*mat:  return M1 * M2   ->   return M2 * M1
```

Reason, in their words: psp2cgc compiles Cg row-major, so the translated GLSL
product comes out transposed. Any matrix maths we push through vitaGL's
translator will hit this too.

## What this means for us

- Our current `vita/src/plat/gx/glx_stubs.cpp` returns dummy objects. The target
  shape is a GX→vitaGL bridge like `vita_gx_cmdbuf.c`, not more stubs.
- We currently link the **stock VitaSDK vitaGL** with default flags and no
  patch. Revisit that when we start rendering for real. Caution: an earlier
  attempt in this port to use the ACGC vitaGL fork regressed even clear-only
  rendering on Vita3K, so change one variable at a time and verify each.
