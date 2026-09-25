#pragma once
/* MW MSL <cmath.h>. The decomp includes it from C as well as C++, so only
   pull the C++ header when we are actually compiling C++. */
#ifdef __cplusplus
#  include <cmath>
#else
#  include <math.h>
#endif
