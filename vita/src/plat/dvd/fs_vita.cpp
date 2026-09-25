#include "plat_abi.h"

#include <vitaGL.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kMaxPath = 512;

} // namespace

bool vita_path_resolve(const char* relative, char* out, size_t out_len)
{
    if (relative == nullptr || relative[0] == '\0' || out == nullptr || out_len == 0) {
        return false;
    }

    // Strip VITA_DATA_ROOT prefix if caller already provided an absolute data path
    constexpr const char kPrefix[] = VITA_DATA_ROOT "/";
    constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
    if (std::strncmp(relative, kPrefix, kPrefixLen) == 0) {
        relative += kPrefixLen;
    }

    // Strip leading path separators to keep game-relative ("art/foo" or "/art/foo")
    while (*relative == '/' || *relative == '\\') {
        relative++;
    }
    if (relative[0] == '\0') {
        return false;
    }

    // Reject parent escape — keep strictly jailed under VITA_DATA_ROOT
    if (std::strstr(relative, "..") != nullptr) {
        return false;
    }

    const int n = std::snprintf(out, out_len, "%s/%s", VITA_DATA_ROOT, relative);
    return n > 0 && static_cast<size_t>(n) < out_len;
}

bool vita_data_present(void)
{
    void* f = vita_file_open("common.ini");
    if (f == nullptr) {
        return false;
    }
    vita_file_close(f);
    return true;
}

bool p0_mount_data(void)
{
    return vita_data_present();
}

// Boot tracing: Vita3K gives us a register dump and nothing else, so append
// every file the game asks for to ux0:data/smstrikers/_vita_fs.log. Cheap, and
// it is the only way to tell "the data is missing" from "the parser is wrong".
void vita_fs_trace(const char* what, const char* detail)
{
    FILE* log = std::fopen(VITA_DATA_ROOT "/_vita_fs.log", "a");
    if (log == nullptr) {
        return;
    }
    std::fprintf(log, "%s %s\n", what, detail != nullptr ? detail : "(null)");
    std::fclose(log);
}

void* vita_file_open(const char* relative_path)
{
    char path[kMaxPath];
    if (!vita_path_resolve(relative_path, path, sizeof(path))) {
        vita_fs_trace("RESOLVE-FAIL", relative_path);
        return nullptr;
    }
    FILE* f = std::fopen(path, "rb");
    vita_fs_trace(f != nullptr ? "OPEN" : "MISS", relative_path);
    return f;
}

void vita_file_close(void* file)
{
    if (file != nullptr) {
        std::fclose(static_cast<FILE*>(file));
    }
}

size_t vita_file_read(void* file, void* buf, size_t n)
{
    if (file == nullptr || buf == nullptr) {
        return 0;
    }
    const size_t got = std::fread(buf, 1, n, static_cast<FILE*>(file));
    if (got != n) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "short read: wanted %lu got %lu",
                      (unsigned long)n, (unsigned long)got);
        vita_fs_trace("READ", msg);
    }
    return got;
}

long vita_file_size(void* file)
{
    if (file == nullptr) {
        return -1;
    }
    FILE* f = static_cast<FILE*>(file);
    const long cur = std::ftell(f);
    if (cur < 0) {
        return -1;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    const long end = std::ftell(f);
    std::fseek(f, cur, SEEK_SET);
    return end;
}

int vita_file_seek(void* file, long offset, int whence)
{
    if (file == nullptr) {
        return -1;
    }
    return std::fseek(static_cast<FILE*>(file), offset, whence);
}

long vita_file_tell(void* file)
{
    if (file == nullptr) {
        return -1;
    }
    return std::ftell(static_cast<FILE*>(file));
}

void vita_fs_fatal_missing_common_ini(void)
{
    std::fprintf(stderr, "\n==================================================\n");
    std::fprintf(stderr, "[FATAL] Missing required game file: %s/common.ini\n", VITA_DATA_ROOT);
    std::fprintf(stderr, "[FATAL] Please copy game assets into %s/\n", VITA_DATA_ROOT);
    std::fprintf(stderr, "==================================================\n\n");

    std::printf("\n==================================================\n");
    std::printf("[FATAL] Missing required game file: %s/common.ini\n", VITA_DATA_ROOT);
    std::printf("[FATAL] Please copy game assets into %s/\n", VITA_DATA_ROOT);
    std::printf("==================================================\n\n");

    if (glplatIsInitialized()) {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.85f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        vglSwapBuffers(GL_FALSE);
    }
}

bool vita_ensure_data_or_fatal(void)
{
    if (!vita_data_present()) {
        vita_fs_fatal_missing_common_ini();
        return false;
    }
    return true;
}
