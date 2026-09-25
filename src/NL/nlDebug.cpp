#include "NL/nlDebug.h"

#include "types.h"

#ifdef TARGET_VITA
#include <psp2/io/fcntl.h>
#include <stdio.h>
#include <string.h>
#endif

/**
 * Offset/Address/Size: 0x0 | 0x801CE948 | size: 0xC
 */
void nlBreak()
{
#ifdef TARGET_VITA
    // GC wrote *(u32*)1 to halt; on Vita that kills Vita3K's host process.
    // Log a breadcrumb and return so FE can limp instead of vanishing.
    const int fd = sceIoOpen("ux0:data/smstrikers/_vita_status.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0)
    {
        const char* msg = "nlBreak\n";
        sceIoWrite(fd, msg, 8);
        sceIoClose(fd);
    }
#else
    *(u32*)1 = 0;
#endif
}
