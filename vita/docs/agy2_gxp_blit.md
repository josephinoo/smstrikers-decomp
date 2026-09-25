# GXP Blit Digest (ACGC-aligned)

## Files Changed
- `vita/src/plat/gx/gxp_simple.h` — `gxp_vertex` (2236) + `gxp_simple_v0` (788); MIT FlyingMeta / Brendonm17
- `vita/src/plat/gx/bnr_title.cpp` — VS+FS link like ACGC `vita_load_simple`; NDC TitVtx + uniforms + VBO

## Key Decisions
- Same pair as ACGC `vita_get_simple_shader()`: shared `gxp_vertex` + `gxp_simple_v0` via `glShaderBinary` (4-byte header) + `glBindAttribLocation` 0–3.
- Draw path mirrors `vita_banner.c`: identity `u_projection`/`u_modelview`, `u_use_texture0=1`, attrib layout matching Vita `PCGXVertex`.
- Fragment-only / client-state path removed — that was incomplete vs ACGC.

## Context for next step
Wire `gc_decode_rgb5a3` into banner load (drop local decoder), then FE texture uploads through the same GXP simple program.

## Vita3K note (2026-09-23)
Stock Vita3K + SDK libvitaGL: `glLinkProgram` on ACGC `gxp_vertex`+`gxp_simple_v0` hard-crashes (MemoryRead). Title uses `kTryGxpBlit=false` + scissor RLE of `gc_decode_rgb5a3` output until Brendonm17/vitaGL or hardware.
