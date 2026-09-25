#include "plat_abi.h"

#include <cstdio>
#include <cstdint>

// Forward declarations for engine GL startup and texture loading
bool glStartup(void);
// glLoadTextureBundle lives in glplat_texbundle.cpp (real PTLG loader)

// Optional hook for NL config parser. If a future unit links an nlConfig shim,
// this weak symbol will resolve to it; otherwise it safely evaluates to nullptr.
extern "C" __attribute__((weak)) bool nl_config_load_file(const char* filename);

// Internal state machine stages for logging
typedef enum VitaBootStage {
    VITA_BOOT_STAGE_INIT = 0,
    VITA_BOOT_STAGE_MOUNT_DATA,
    VITA_BOOT_STAGE_LOAD_CONFIG,
    VITA_BOOT_STAGE_GL_STARTUP,
    VITA_BOOT_STAGE_GLOBAL_TEXTURES,
    VITA_BOOT_STAGE_COMPLETE,
    VITA_BOOT_STAGE_FAILED
} VitaBootStage;

static const char* stage_to_str(VitaBootStage stage)
{
    switch (stage) {
    case VITA_BOOT_STAGE_INIT:
        return "INIT";
    case VITA_BOOT_STAGE_MOUNT_DATA:
        return "MOUNT_DATA";
    case VITA_BOOT_STAGE_LOAD_CONFIG:
        return "LOAD_CONFIG";
    case VITA_BOOT_STAGE_GL_STARTUP:
        return "GL_STARTUP";
    case VITA_BOOT_STAGE_GLOBAL_TEXTURES:
        return "GLOBAL_TEXTURES";
    case VITA_BOOT_STAGE_COMPLETE:
        return "COMPLETE";
    case VITA_BOOT_STAGE_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

const char* vita_game_boot_status_str(VitaGameBootStatus status)
{
    switch (status) {
    case VITA_BOOT_OK:
        return "VITA_BOOT_OK";
    case VITA_BOOT_DATA_MISSING:
        return "VITA_BOOT_DATA_MISSING";
    case VITA_BOOT_CONFIG_ERROR:
        return "VITA_BOOT_CONFIG_ERROR";
    case VITA_BOOT_GL_ERROR:
        return "VITA_BOOT_GL_ERROR";
    case VITA_BOOT_TEXTURE_ERROR:
        return "VITA_BOOT_TEXTURE_ERROR";
    default:
        return "VITA_BOOT_UNKNOWN";
    }
}

// Engine stub: glStartup -> returns true
bool glStartup(void)
{
    // glplatStartup and vglInitExtended are already active on Vita
    return true;
}

VitaGameBootStatus vita_game_boot(void)
{
    std::printf("========================================\n");
    std::printf("[VITA BOOT] Starting thin game boot slice\n");
    std::printf("========================================\n");

    VitaBootStage stage = VITA_BOOT_STAGE_INIT;
    bool data_mounted = false;

    // Step 1: Mount data
    stage = VITA_BOOT_STAGE_MOUNT_DATA;
    std::printf("[BOOT][STATE: %s] Probing game data mount at '%s'...\n",
                stage_to_str(stage), VITA_DATA_ROOT);
    data_mounted = p0_mount_data();
    if (data_mounted) {
        std::printf("[BOOT][STATE: %s] Data root verified (common.ini present)\n",
                    stage_to_str(stage));
    } else {
        std::printf("[BOOT][STATE: %s] Warning: common.ini not found (amber mode / missing data)\n",
                    stage_to_str(stage));
    }

    // Step 2: Optional Config load if NL shim present
    stage = VITA_BOOT_STAGE_LOAD_CONFIG;
    std::printf("[BOOT][STATE: %s] Evaluating configuration subsystem...\n",
                stage_to_str(stage));
    if (nl_config_load_file != nullptr) {
        std::printf("[BOOT][STATE: %s] NL Config shim detected; loading INI configuration files...\n",
                    stage_to_str(stage));
        const char* inis[] = {"common.ini", "platform.ini", "locale.ini", "user.ini"};
        for (const char* ini : inis) {
            const bool ok = nl_config_load_file(ini);
            std::printf("[BOOT][STATE: %s]   Loaded %s: %s\n",
                        stage_to_str(stage), ini, ok ? "OK" : "FAILED");
        }
    } else {
        std::printf("[BOOT][STATE: %s] No NL Config shim linked; probing INI files via VFS...\n",
                    stage_to_str(stage));
        const char* inis[] = {"common.ini", "platform.ini", "locale.ini", "user.ini"};
        for (const char* ini : inis) {
            void* f = vita_file_open(ini);
            if (f != nullptr) {
                const long sz = vita_file_size(f);
                vita_file_close(f);
                std::printf("[BOOT][STATE: %s]   %s found (%ld bytes)\n",
                            stage_to_str(stage), ini, sz);
            } else {
                std::printf("[BOOT][STATE: %s]   %s missing (optional)\n",
                            stage_to_str(stage), ini);
            }
        }
    }

    // Step 3: Stub glStartup -> true
    stage = VITA_BOOT_STAGE_GL_STARTUP;
    std::printf("[BOOT][STATE: %s] Invoking glStartup()...\n",
                stage_to_str(stage));
    const bool gl_ok = glStartup();
    if (!gl_ok) {
        std::printf("[BOOT][STATE: %s] FAILED: glStartup returned false\n",
                    stage_to_str(stage));
        return VITA_BOOT_GL_ERROR;
    }
    std::printf("[BOOT][STATE: %s] glStartup() returned true\n",
                stage_to_str(stage));

    // Step 4: Skip global.glt OR stub true
    stage = VITA_BOOT_STAGE_GLOBAL_TEXTURES;
    std::printf("[BOOT][STATE: %s] Loading global texture bundle (global.glt)...\n",
                stage_to_str(stage));
    const bool textures_ok = glLoadTextureBundle("global.glt");
    if (!textures_ok) {
        std::printf("[BOOT][STATE: %s] FAILED: glLoadTextureBundle returned false\n",
                    stage_to_str(stage));
        return VITA_BOOT_TEXTURE_ERROR;
    }
    std::printf("[BOOT][STATE: %s] Global texture bundle initialized (stub -> true)\n",
                stage_to_str(stage));

    // Complete
    stage = VITA_BOOT_STAGE_COMPLETE;
    std::printf("========================================\n");
    std::printf("[BOOT][STATE: %s] Thin game boot slice finished successfully\n",
                stage_to_str(stage));
    std::printf("========================================\n");

    if (!data_mounted) {
        return VITA_BOOT_DATA_MISSING;
    }
    return VITA_BOOT_OK;
}
