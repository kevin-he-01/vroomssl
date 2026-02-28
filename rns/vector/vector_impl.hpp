#pragma once

// Conditional inclusion of AVX512 or fallback implementation
// This header provides a unified interface for AVXVector

#ifdef USE_AVX512_FALLBACK
    // Use fallback implementation when AVX512 is not available
    #include "fallback.hpp"
#else
    // Use AVX512 implementation by default
    #include "avx512ifma.hpp"
#endif

// The AVXVector class is now available regardless of which implementation is used
