#include <stddef.h>
#include <stdint.h>

// Match splashscreen.c: never set active — gxm.c skips sema dance when 0.
uint8_t is_splashscreen_active = 0;
int32_t splash_mutex[2] = {0, 0};

void invoke_splashscreen(void) {}
void clear_splashscreen(void) {}

void* vglGetCaveBuffer(size_t* sz)
{
    if (sz != NULL) {
        *sz = 0;
    }
    return NULL;
}
