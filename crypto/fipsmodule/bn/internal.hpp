#pragma once
#include <openssl/bn.h>

#include <assert.h>
#include <stdlib.h>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "internal.h"

#include "../rns/crypto/ring_impl_header.hpp"

#if defined(USE_DRNS_RING)
  template<int bits>
  DRNSRing<bits> *bn_mont_ctx_get_ring(const BN_MONT_CTX *mont) {
    return (DRNSRing<bits> *)mont->ring_ptr;
  }
#else
  template<int bits, typename RingType = Ring<bits>, bool drns = false>
  RingType *bn_mont_ctx_get_ring(const BN_MONT_CTX *mont) {
    if constexpr (drns) {
      return (RingType *)mont->drns_ring_ptr;
    } else {
      return (RingType *)mont->ring_ptr;
    }
  }
#endif

