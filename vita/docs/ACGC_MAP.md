# Cómo lo hizo ACGC (`vita` branch) → qué falta en SMS

Repo: https://github.com/Brendonm17/ACGC-Vita-Port/tree/vita

## Lo que ACGC hace (el juego de verdad)

```
vita/src/vita_main.c
  → pc_platform_init / disc / assets
  → ac_entry()     // src/main.c  con -Dmain=ac_entry
  → boot_main()    // src/static/boot.c con -Dmain=boot_main
```

CMake **GLOBea todo** `src/*.c` + `src/*.cpp`, **excluye** `dolphin/os|gx|dvd|pad|...`, y enlaza la capa PC:

- `pc/src/pc_gx*.c`, `pc_dvd.c`, `pc_os.c`, `pc_pad.c`, …
- Vita: `vita_gx_cmdbuf.c`, `vita_frame.c`, …
- **GXP precompilados** en `vita/include/vita_gxp_shaders.h` (psp2cgc) → **no** hace falta `libshacccg` día a día
- VitaGL fork: `Brendonm17/vitaGL` `@async-compressed-tex-prep` (`vita/rebuild_vitagl.sh`)

Datos: `ux0:data/AnimalCrossing/rom/` (ISO).

## Equivalente SMS

| ACGC | SMS |
|------|-----|
| `ac_entry` + `boot_main` | `src/Game/main.cpp` `Initialize` + `main` → renombrar a `sms_entry` |
| GLOB `src/*` excl. dolphin | GLOB `src/Game` + `src/NL` + `src/ode`; excl. `src/Dolphin`, GC `glx`/`glPlat`/`nlFileGC` |
| `pc_gx` / cmdbuf | NL ya tiene `gl`/`glx` ABI → implementar `glplat*` / `glx*` en `vita/src/plat/gx/` |
| GXP headers | Ya copiados en `vita/src/plat/gx/gxp_simple.h` + `vita_gxp_blit.cpp` |
| `ux0:data/AnimalCrossing/rom/` | `ux0:data/smstrikers/` (dump G4QE01) |
| vitaGL fork | **Obligatorio** para GXP estable; stock Vita3K host-crashea en `glShaderBinary`/`glLinkProgram` |

## Lecciones aplicadas de ACGC (rama `vita`)

| ACGC | SMS |
|------|-----|
| `pc/src/pc_card.c`: `CARDMountAsync` / `CheckAsync` **disparan el callback** al momento | Antes: stubs `return 0` sin callback → hang en “Checking Memory Card”. Ahora: mismo patrón en `vita/src/plat/dolphin_stubs.cpp` |
| `pc_card.c` `CARDProbeEx` rellena size/sector; `FreeBlocks` da espacio libre | Igual (antes dejaba outs sin escribir) |
| GXP committed + fork vitaGL | SMS ya tiene GXP; stock Vita3K sigue frágil con `glShaderBinary` |
| Save en `ux0:data/.../saves/card_a` | Pendiente (Open → `NOFILE` basta para pasar el check) |

## Estado ahora

- `sms_entry` + H&S + loc endian + pragma pack FE ✅
- CARD callbacks → pasa `saving_loading` → carga `englegal` / `movieplayer` / `start_screen_v2` ✅
- TDEV skip movies + FE anim null rings ✅
- **fen `FEAnimation::m_cast_type` u16 fix** — sin esto v3 anims se leen como scalar y `m_next` = float (4.8f) → ~4k faults

## Siguiente (orden ACGC)

1. Verificar title screen visuals tras cast_type fix
2. Arreglar ghost atlas residual en FE text
3. Save file-backed tipo `pc_card.c` bajo `ux0:data/smstrikers/saves/`
