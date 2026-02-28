#pragma once

constexpr int ceil_log2(int x) {
    return (x == 0) ? 0 : 32 - __builtin_clz(x);
}

#ifdef DEBUG_RNS_NOINLINE
#define INLINE
#else
#define INLINE __attribute__((always_inline))
#endif
