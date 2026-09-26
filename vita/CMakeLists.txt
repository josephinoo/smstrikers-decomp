cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
    else()
        message(FATAL_ERROR "Set VITASDK before configuring.")
    endif()
endif()

project(SMStrikersVita LANGUAGES C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "SM Strikers Vita")
set(VITA_TITLEID  "VSTR00002")
set(VITA_VERSION  "00.14")

set(DECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_compile_definitions(
    TARGET_VITA
    TARGET_PC
    __vita__
    dSINGLE
    LITTLE_ENDIAN
    __IEEE_LITTLE_ENDIAN
    VITA_ENABLE_GXP=0
    MUSY_TARGET=MUSY_TARGET_PC
)

set(VITA_SOURCES
    src/main.cpp
    src/plat/dvd/fs_vita.cpp
    src/plat/gx/glplat_stub.cpp
    src/plat/gx/gc_tex_decode.cpp
    src/plat/gx/bnr_title.cpp
    src/plat/gx/vita_gxp_blit.cpp
    src/plat/gx/menu_screen.cpp
)

add_executable(smstrikers_vita ${VITA_SOURCES})

target_include_directories(smstrikers_vita PRIVATE
    include
    src/plat/gx
    ${DECOMP_ROOT}/include
    ${DECOMP_ROOT}/src
    ${DECOMP_ROOT}/src/ode
    ${DECOMP_ROOT}/extern/musyx/include
    ${DECOMP_ROOT}/extern/dolphin/include
    $ENV{VITASDK}/arm-vita-eabi/include
)

target_compile_features(smstrikers_vita PRIVATE cxx_std_17)
target_compile_options(smstrikers_vita PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/vita_mw_compat.h
)

# Port C++ (exclude .c so they don't get -std=gnu++17)
set(VITA_CXX_SOURCES ${VITA_SOURCES})
list(FILTER VITA_CXX_SOURCES EXCLUDE REGEX "\\.c$")
set_source_files_properties(${VITA_CXX_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-unused-parameter"
)

target_link_libraries(smstrikers_vita PRIVATE
    vitaGL
    vitashark
    SceShaccCgExt
    taihen_stub
    SceShaccCg_stub
    mathneon
    SceKernelDmacMgr_stub
    SceCtrl_stub
    SceDisplay_stub
    SceGxm_stub
    SceCommonDialog_stub
    SceSysmodule_stub
    SceAppMgr_stub
    SceAppUtil_stub
    SceIofilemgr_stub
    SceLibKernel_stub
    SceKernelThreadMgr_stub
    SceSysmem_stub
    m
    stdc++
)

vita_create_self(eboot.bin smstrikers_vita UNSAFE)
vita_create_vpk(smstrikers-vita.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
)

# ---------------------------------------------------------------------------
# Real game code (ACGC-style GLOB of the portable decomp)
# Object library only for now: `make smsgame` iterates compile errors without
# breaking the working demo eboot.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SMS_GAME_SOURCES
    ${DECOMP_ROOT}/src/Game/*.cpp
    ${DECOMP_ROOT}/src/Game/*.c
    ${DECOMP_ROOT}/src/NL/*.cpp
    ${DECOMP_ROOT}/src/NL/*.c
    ${DECOMP_ROOT}/src/ode/*.cpp
    ${DECOMP_ROOT}/src/ode/*.c
)
# GameCube-only backends: Vita reimplements these under vita/src/plat/.
# GameCube GX backend: the Vita reimplements this over vitaGL.
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/NL/glx/")
# plataudio.cpp is MusyX/ARAM and platvmath.cpp is paired-single PPC asm; both
# are replaced under vita/src/plat/. Everything else in src/NL/plat and all of
# src/NL/gc (quaternion math, the pad, the texture swizzler) is portable and
# builds as-is, so we use the decomp's real code rather than stubbing it.
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/NL/plat/(plataudio|platvmath)\\.cpp$")
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/NL/(nlFileGC)\\.cpp$")
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/NL/gl/glPlat\\.cpp$")
# ODE files the upstream library itself marks "should not be compiled".
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/ode/(stack|scrapbook)\\.cpp$")
# ODE keeps *_orig.cpp copies of ode.cpp and joint.cpp for reference; linking
# both copies duplicates every symbol in them.
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/ode/.*_orig\\.cpp$")
# ODE trimesh collision needs the OPCODE library, which is not in this repo
# and which SM Strikers does not use (its geoms are box/sphere/ccylinder).
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/ode/collision_trimesh")
# Decomp scratch artifacts: interpreter dumps and AI test scaffolding that
# were never part of the shipped build.
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "_interp\\.cpp$")
list(FILTER SMS_GAME_SOURCES EXCLUDE REGEX "/src/Game/AI/Scripts/TestCases\\.cpp$")

add_library(smsgame OBJECT ${SMS_GAME_SOURCES})
target_include_directories(smsgame PRIVATE
    include
    src/plat/gx
    ${DECOMP_ROOT}/include
    ${DECOMP_ROOT}/src
    ${DECOMP_ROOT}/src/ode
    ${DECOMP_ROOT}/extern/musyx/include
    ${DECOMP_ROOT}/extern/dolphin/include
    $ENV{VITASDK}/arm-vita-eabi/include
)
target_compile_features(smsgame PRIVATE cxx_std_17)
target_compile_options(smsgame PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/vita_mw_compat.h
    -w
    -fpermissive
    -g
    # Metrowerks on PowerPC gives every enum 4 bytes, and the decomp's struct
    # layouts (and its documented field offsets) assume that. ARM EABI defaults
    # to -fshort-enums, which silently shrinks enums to 1 byte and shifts those
    # layouts. Match the original ABI instead of patching enums one at a time.
    -fno-short-enums
    # dolphin/os.h has tentative definitions (__OSExecParams,
    # __OSAppLoaderOffset). Metrowerks merged them; modern GCC defaults to
    # -fno-common and would make each TU's copy a hard definition.
    -fcommon
)

# ODE's fast*.c reach into NL headers, which are C++. Metrowerks compiled the
# whole of ODE as C++ too.
set(SMS_ODE_C_AS_CXX ${SMS_GAME_SOURCES})
list(FILTER SMS_ODE_C_AS_CXX INCLUDE REGEX "\\.c$")
set_source_files_properties(${SMS_ODE_C_AS_CXX} PROPERTIES LANGUAGE CXX)

# ---------------------------------------------------------------------------
# sms_entry: the real game. Links the GLOB'd decomp against the Vita platform
# layer. Kept as its own executable so an unresolved symbol here never breaks
# the runnable demo VPK above.
# ---------------------------------------------------------------------------
set(SMS_PLAT_SOURCES
    src/plat/dolphin_stubs.cpp
    src/plat/musyx_stubs.cpp
    src/plat/plataudio_stubs.cpp
    src/sms_entry_main.cpp
    src/plat/dvd/fs_vita.cpp
    src/plat/dvd/nl_fs_shim.cpp
    src/plat/gx/glplat_stub.cpp
    src/plat/gx/glx_stubs.cpp
    ${DECOMP_ROOT}/src/NL/glx/glxLoadModel.cpp
    ${DECOMP_ROOT}/src/NL/glx/glxDisplayList.cpp
    src/plat/gx/gc_tex_decode.cpp
    src/plat/gx/glplat_texbundle.cpp
    src/plat/gx/vita_gxp_blit.cpp
    src/plat/gx/vgl_compat.cpp
    src/plat/gx/vgl_splash_stub.c
    src/plat/audio/audio_stub.cpp
    src/plat/math/platvmath_c.cpp
    src/plat/math/vmath_vita.cpp
    src/plat/os/os_vita.cpp
    src/plat/pad/pad_vita.cpp
    src/plat/link_stubs.cpp
    src/plat/nl_plat_impl.cpp
    src/plat/mw_runtime.cpp
    src/plat/game_gaps.cpp
    src/plat/render_trace.cpp
)

add_executable(sms_entry ${SMS_PLAT_SOURCES} $<TARGET_OBJECTS:smsgame>)
target_include_directories(sms_entry PRIVATE
    include
    src/plat/gx
    ${DECOMP_ROOT}/include
    ${DECOMP_ROOT}/src
    ${DECOMP_ROOT}/src/ode
    ${DECOMP_ROOT}/extern/musyx/include
    ${DECOMP_ROOT}/extern/dolphin/include
    $ENV{VITASDK}/arm-vita-eabi/include
)
target_compile_features(sms_entry PRIVATE cxx_std_17)
target_compile_options(sms_entry PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/vita_mw_compat.h
    -w
    # The game headers declare Metrowerks-shaped operator new overloads.
    -fpermissive
    -g
    -fno-short-enums
    # See the smsgame note: dolphin/os.h relies on tentative definitions.
    -fcommon
)
target_link_options(sms_entry PRIVATE
    -Wl,--undefined=_newlib_heap_size_user
    -Wl,--undefined=sceUserMainThreadStackSize
    -Wl,--wrap=_ZN8FERender11RenderSceneEP7FEScene
    -Wl,--wrap=_ZN8FERender18RenderPresentationEPK14FEPresentation
    -Wl,--wrap=_ZN8FERender19RenderImageInstanceEPK15TLImageInstance
    -Wl,--wrap=_Z17glViewAttachModel7eGLViewmPK7glModel
    -Wl,--wrap=_ZN12GLRenderList11AttachModelEPK7glModelm
    -Wl,--wrap=_ZNK9FEPackage15GetPresentationEv
)
target_link_libraries(sms_entry PRIVATE
    vitaGL vitashark SceShaccCgExt taihen_stub SceShaccCg_stub mathneon
    SceKernelDmacMgr_stub SceCtrl_stub SceDisplay_stub SceGxm_stub
    SceCommonDialog_stub SceSysmodule_stub SceAppMgr_stub SceAppUtil_stub
    SceIofilemgr_stub SceLibKernel_stub SceKernelThreadMgr_stub SceSysmem_stub
    m stdc++
)
set_target_properties(sms_entry PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Its own title id so it can be installed next to the demo shell rather than
# replacing a build we know runs.
vita_create_self(sms_entry.bin sms_entry UNSAFE)
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")
vita_create_vpk(smstrikers-game.vpk VSTR00003 sms_entry.bin
    VERSION 01.00
    NAME "SM Strikers (game)"
)
set_target_properties(sms_entry.bin-self smstrikers-game.vpk-vpk
    PROPERTIES EXCLUDE_FROM_ALL TRUE)

# The decomp's own main() becomes sms_entry().
set_source_files_properties(${DECOMP_ROOT}/src/Game/main.cpp
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES COMPILE_DEFINITIONS "main=sms_entry"
)
