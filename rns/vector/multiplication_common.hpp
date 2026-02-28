#pragma once
#include "../crypto/common.hpp"
#include "changebase.hpp"
#include "vector_impl.hpp"
#include "../precompute/precompute.hpp"
#include "../reduction/montgomery.hpp"
#include "conversion.hpp"
#include <cstdio>
#include <cstdlib>

template <template <int> class Reduction, int limbs>
// r = (a * b) mod M
INLINE
inline AVXVector<limbs> ele_mult(const AVXVector<limbs> &a, const AVXVector<limbs> & b, const Reduction<limbs> &r) {
    auto ab_lo = AVXVector<limbs>(0).mullo(a, b);
    auto ab_hi = AVXVector<limbs>(0).mulhi(a, b);
    return r.reduce_small(ab_hi, ab_lo);
}

template <template <int> class Reduction, int limbs, size_t batch>
// r[i] = (a[i] * b[i]) mod M
INLINE
inline std::array<AVXVector<limbs>, batch> batch_ele_mult(const std::array<AVXVector<limbs>, batch> &a, const std::array<AVXVector<limbs>, batch> &b, const std::array<Reduction<limbs>, batch> &r) {
    std::array<AVXVector<limbs>, batch> ab_lo;
    std::array<AVXVector<limbs>, batch> ab_hi;
    for (size_t i = 0; i < batch; i++) {
        ab_lo[i] = AVXVector<limbs>(0).mullo(a[i], b[i]);
        ab_hi[i] = AVXVector<limbs>(0).mulhi(a[i], b[i]);
    }
    return Reduction<limbs>::template reduce_small_batch_diff_moduli<batch>(r, ab_hi, ab_lo);
}

template <template <int> class Reduction, int limbs>
// r = (a * b + c * d) mod M
INLINE
inline AVXVector<limbs> ele_sum_prod(const AVXVector<limbs> &a, const AVXVector<limbs> & b, const AVXVector<limbs> &c, const AVXVector<limbs> &d, const Reduction<limbs> &r) {
    auto ab_lo = AVXVector<limbs>(0).mullo(a, b);
    auto ab_hi = AVXVector<limbs>(0).mulhi(a, b);
    auto r_lo = ab_lo.mullo(c, d);
    auto r_hi = ab_hi.mulhi(c, d);
    return r.reduce_full(r_hi, r_lo); // seems necessary, reduce_small would not work. Is it significantly slower?
    // Is there a faster way to do this? (simple modification of reduce_small that tolerates higher redundance)
}

// TODO: batched ele_sum_prod
