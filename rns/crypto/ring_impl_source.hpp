#pragma once

#include "ring_impl_header.hpp"

#if defined(USE_DRNS_RING)
    #define Ring DRNSRing
#else
    #define Ring Ring
#endif
