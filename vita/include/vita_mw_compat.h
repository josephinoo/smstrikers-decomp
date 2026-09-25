/* MetroWerks / GameCube shims for Vita (GCC) — force-included on all TUs. */
#pragma once

#ifdef __cplusplus
#  if __cplusplus >= 201103L
#    include <cmath>
#    include <cstdint>
#    include <cstdlib>
using f128 = long double;
#    ifndef __fabs
#      define __fabs(x) std::fabs(x)
#    endif
#    ifndef __fabsf
#      define __fabsf(x) std::fabsf(x)
#    endif
#    ifndef __abs
#      define __abs(x) std::abs(x)
#    endif
#    ifndef __labs
#      define __labs(x) std::labs(x)
#    endif
#  else
#    include <math.h>
#    include <stdint.h>
#    include <stdlib.h>
typedef long double f128;
#    ifndef __fabs
#      define __fabs(x) fabs(x)
#    endif
#    ifndef __fabsf
#      define __fabsf(x) fabsf(x)
#    endif
#    ifndef __abs
#      define __abs(x) abs(x)
#    endif
#    ifndef __labs
#      define __labs(x) labs(x)
#    endif
#  endif
#else
#  include <math.h>
#  include <stdint.h>
#  include <stdlib.h>
typedef long double f128;
#  ifndef __fabs
#    define __fabs(x) fabs(x)
#  endif
#  ifndef __fabsf
#    define __fabsf(x) fabsf(x)
#  endif
#endif

#include <string.h>

#include <stdarg.h>

/* MSL ctype internals, mapped onto newlib's table. */
#ifdef __cplusplus
extern "C" const char _ctype_[];
#else
extern const char _ctype_[];
#endif
#define __digit 04
#define __ctype_map (_ctype_ + 1)

/* PPC count-leading-zeros. */
#ifndef __cntlzw
#define __cntlzw(x) ((x) ? __builtin_clz((unsigned int)(x)) : 32)
#endif

/* PPC reciprocal-square-root estimate. The Vita has no equivalent
   instruction, so use the exact value; callers only ever refine it. */
static inline double __frsqrte(double x) { return 1.0 / sqrt(x); }

#ifndef __IEEE_LITTLE_ENDIAN
#define __IEEE_LITTLE_ENDIAN 1
#endif

#ifndef FORCE_DONT_INLINE
#define FORCE_DONT_INLINE \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0
#endif

#ifndef __declspec
#define __declspec(x)
#endif

#ifndef WEAKFUNC
#define WEAKFUNC
#endif
#ifndef DECL_SECT
#define DECL_SECT(name)
#endif
#ifndef ASM
#define ASM
#endif
#ifndef INIT
#define INIT
#endif

#ifndef __memcpy
#define __memcpy memcpy
#endif
#ifndef __memset
#define __memset memset
#endif
#ifndef __memmove
#define __memmove memmove
#endif
#include <alloca.h>
#undef __alloca
#define __alloca alloca

#include <strings.h>
#ifndef strcmpi
#define strcmpi strcasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifdef __cplusplus
extern "C" {
#endif
void OSReport(const char* message, ...);
extern int32_t __float_max[];
extern int32_t __float_huge[];
extern int32_t __float_nan[];
extern int32_t __double_huge[];
extern int32_t __extended_huge[];
#ifdef __cplusplus
}
#endif
