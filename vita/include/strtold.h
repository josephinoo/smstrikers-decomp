#ifndef _MSL_STRTOLD_H
#define _MSL_STRTOLD_H

#include "types.h"
#include "vita_mw_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

f128 __strtold(int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg, int* chars_scanned, int* overflow);
/* strtol/atof from libc */

#ifdef __cplusplus
}
#endif

#endif
