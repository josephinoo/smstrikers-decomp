#pragma once

#include <cstdint>

unsigned int vita_get_texture(unsigned long handle);
unsigned int vita_get_fallback_texture(void);
void vita_register_texture(unsigned long handle, unsigned int glTex);
void glplatFrameAllocNextFrame(void);
