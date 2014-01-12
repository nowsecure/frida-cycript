#if defined(__cplusplus)
#include <bits/c++config.h>
#if defined(__arm__) && defined(__thumb__)
#undef _GLIBCXX_ATOMIC_BUILTINS
#endif
#undef __TARGETING_4_0_DYLIB
#define __TARGETING_4_0_DYLIB 1
#endif
