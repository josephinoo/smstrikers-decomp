// vgl_compat.cpp - VitaGL compatibility shims and safe wrappers
// Based on ACGC-Vita-Port vita_gx_compat.c workarounds

#include <vitaGL.h>

// Stubs for GL features unsupported on PS Vita / VitaGL

extern "C" void vita_glDrawBuffers(int n, const unsigned int* bufs) {
    (void)n;
    (void)bufs;
}

extern "C" void vita_glReadBuffer(unsigned int mode) {
    (void)mode;
}

// VitaGL crashes when glUniform* is called with location -1 (unbound/optimized-out uniform).
// In desktop OpenGL this is a silent no-op. Guard all uniform calls with location >= 0.

extern "C" void vita_safe_uniform1i(int location, int value) {
    if (location >= 0) glUniform1i(location, value);
}

extern "C" void vita_safe_uniform1f(int location, float value) {
    if (location >= 0) glUniform1f(location, value);
}

extern "C" void vita_safe_uniform2f(int location, float x, float y) {
    if (location >= 0) glUniform2f(location, x, y);
}

extern "C" void vita_safe_uniform3f(int location, float x, float y, float z) {
    if (location >= 0) glUniform3f(location, x, y, z);
}

extern "C" void vita_safe_uniform4f(int location, float x, float y, float z, float w) {
    if (location >= 0) glUniform4f(location, x, y, z, w);
}

extern "C" void vita_safe_uniform4fv(int location, int count, const float* value) {
    if (location >= 0) glUniform4fv(location, count, value);
}

extern "C" void vita_safe_uniform3fv(int location, int count, const float* value) {
    if (location >= 0) glUniform3fv(location, count, value);
}

extern "C" void vita_safe_uniform2fv(int location, int count, const float* value) {
    if (location >= 0) glUniform2fv(location, count, value);
}

extern "C" void vita_safe_uniformMatrix4fv(int location, int count, unsigned char transpose, const float* value) {
    if (location >= 0) glUniformMatrix4fv(location, count, transpose, value);
}

extern "C" void vita_safe_uniformMatrix3fv(int location, int count, unsigned char transpose, const float* value) {
    if (location >= 0) glUniformMatrix3fv(location, count, transpose, value);
}

// glClear() clobbers the active shader program in VitaGL.
// Save and re-bind active program after clear if needed.

static unsigned int s_vita_last_program = 0;

extern "C" void vita_save_program(unsigned int program) {
    s_vita_last_program = program;
}

extern "C" void vita_restore_program_after_clear(void) {
    if (s_vita_last_program != 0) {
        glUseProgram(s_vita_last_program);
    }
}
