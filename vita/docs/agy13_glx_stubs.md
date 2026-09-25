# glx / glplat Stub Digest

## Files Changed
- `vita/src/plat/gx/glx_stubs.cpp` (new file): Added correctly-typed placeholder definitions for missing GameCube GX backend symbols (roughly 50 symbols).
- `vita/CMakeLists.txt`: Appended `src/plat/gx/glx_stubs.cpp` to `SMS_PLAT_SOURCES` so that the executable uses these stubs to link.

## Remaining Undefined Symbols
- Count remaining in scope (`glplat`, `glPlat`, `glx`, `glSetMatrix`, `glSetIgnoreDuplicateModels`): **0**.
- Note: There are still missing symbols for other subsystems (e.g., `PSQUAT*`, `StatsGatherer::*`, `RenderSnapshot::*`), which is expected.

## Key Decisions
- **`glSetMatrix` Definition:** The original codebase's headers declare `void glSetMatrix(unsigned long matrix, const nlMatrix4& m)`, but `src/Game/Drawable/DrawableSkinModel.cpp` illegally forward-declares it as `void glSetMatrix(u32 matrix, const nlMatrix4& m)` (where `u32` is `unsigned int`). Since `unsigned long` and `unsigned int` are distinct types on Vita (manglings differ), I defined `void glSetMatrix(unsigned int matrix, const nlMatrix4& m)` in `glx_stubs.cpp` to satisfy the missing linker symbol without triggering a multiple definition error against `src/NL/gl/glMatrix.cpp` (which provides the `unsigned long` version).
- **Avoiding Duplicates:** Carefully skipped stubbing functions like `glplatBeginFrame`, `glplatStartup`, etc., since they were already defined in `vita/src/plat/gx/glplat_stub.cpp`.
- **Typing Accuracy:** Included the real headers (`NL/gl/glPlat.h`, `NL/glx/*.h`) in the stub file to enforce exact type matches.

## Placeholders Flagged as Crash Risks
The following functions return `nullptr` (or a similar zero handle) which is highly likely to crash the game during initialization or the main loop if the caller dereferences the result without checking:
- `glplatLoadModel`, `glplatEndLoadModel`: Returning `glModel*` as `nullptr`.
- `glx_MakeSkinMesh`: Returning `GLSkinMesh*` as `nullptr`.
- `glx_CreatePlatTexture`, `glx_GetTex`: Returning `PlatTexture*` as `nullptr`.
- `glplatFrameAlloc`, `glplatResourceAlloc`: Returning `void*` (memory blocks) as `nullptr`.
- `glxGetBackBuffer`, `glxGetDisplayedBuffer`: Returning `void*` as `nullptr`.

## Context for Next Step
With the initial linking step complete for the GX backend symbols, the next phase can begin replacing these placeholders with the actual vitaGL implementations. Since the build is currently configured to link using `smsgame` (which contains unexcluded portable decompilation code), `sms_entry` will likely encounter `NULL` dereferences when booting. The parallel engineer's work to stub out remaining missing symbols (`StatsGatherer`, `PSQUAT`, etc.) will close the final link errors, allowing us to formally debug the boot sequence and incrementally swap out these placeholders for functional code.
