#pragma once
#include "common.hpp"
#include "../vector/multiplication.hpp"
#include "../precompute/gmp_wrapper.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

// Hold the multiplication, addition, subtraction info and algorithms
// Hold one copy of the moduli
// Hold conversion to and from algorithms


template<int modulus_bits, int num_additions=0>
class Ring {

    // Estimate for how much bigger the RNS needs to be than the modulus
    static constexpr int max_bound = (3 + num_additions)*(3 + num_additions);
    static constexpr int element_modbits = 52;  
    static constexpr int limbs = ceil_div(modulus_bits + ceil_log2(max_bound), element_modbits);
    const IntRNS2System<modulus_bits, limbs> multiplier;
    
    public:
    static constexpr int modbits = modulus_bits;

    struct RingElementSmall {
        AVXVector<limbs> r_m2;
    };
    
    struct RingElement {
        AVXVector<limbs> r_m1;
        AVXVector<limbs> r_m2;
    };

    struct RingElementWide {
       RingElement hi;
       RingElement lo;
    };

    RingElement target_in_rns;
    RingElement _one;

    inline Ring(const BigInt &modulus) : multiplier(make_IntRNS2SystemMontgomery<modulus_bits, limbs>(modulus, element_modbits, max_bound)) {
        // Measure constructor running time
        // auto start_time = std::chrono::high_resolution_clock::now();
        if (modulus.bit_length() > modulus_bits) {
            fprintf(stderr, "Error: Please compile ring with correct number of bits for modulus: Expected %d but got %zu bits\n", modulus_bits, modulus.bit_length());
            abort();
        }
        auto [one_m1, one_m2] = multiplier.to_mont_avx(BigInt(1));
        _one = {one_m1, one_m2};
        auto [target_m1, target_m2] = multiplier.to_mont_avx(modulus * BigInt(max_bound));
        target_in_rns = {target_m1, target_m2};
        // auto end_time = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        // fprintf(stderr, "Ring constructor for modulus %zu bits took %zu microseconds\n", modulus.bit_length(), duration.count());
    }

    inline RingElement zero() const {
        RingElement r = {
            AVXVector<limbs>(0),
            AVXVector<limbs>(0)
        };
        return r;
    }

    inline RingElement one() const {
        return _one;
    }

    inline RingElement modulus() const {
        return target_in_rns;
    }

    // For repeated addition; non-modular multiplication.
    inline RingElement scalar(uint32_t s) const {
        RingElement r = {
            AVXVector<limbs>(s),
            AVXVector<limbs>(s)
        };
        return r;
    }

    INLINE
    inline RingElement from_int(const BigInt &i) const {
        auto [r_m1, r_m2] = multiplier.to_mont_avx(i);
        RingElement r = {r_m1, r_m2};
        return r;
    }

    INLINE
    inline BigInt to_int(const RingElement &r) const {
        return multiplier.from_mont_avx(r.r_m2);
    }

    INLINE
    inline RingElement mul(const RingElement &a, const RingElement &b) const {
        RingElement out;
        multiplier.mul_reduce(a.r_m1, a.r_m2, b.r_m1, b.r_m2, out.r_m1, out.r_m2);
        return out;
    }

    INLINE
    inline RingElement square(const RingElement &a) const {
        return mul(a, a);
    }

    inline RingElement neg(const RingElement &a) const {
        // return k*p - x, for x < k*p (so static bound checking is important here)
        return sub(target_in_rns, a);
    }

    // for Lazy reduction, multiply, add, and then reduce
    // inline RingElement raw_mul(const RingElement &a, const RingElement &b) const {
    //     RingElement out;
    //     multiplier.raw_mul(a.r_m1, a.r_m2, b.r_m1, b.r_m2, out.r_m1, out.r_m2);
    //     return out;
    // }

    INLINE
    inline RingElement ctime_select(const bool selection, const RingElement &a, const RingElement &b) const {
        RingElement out;
        out.r_m1 = a.r_m1.ctime_select(selection, b.r_m1);
        out.r_m2 = a.r_m2.ctime_select(selection, b.r_m2);
        return out;
    }

    INLINE
    static inline RingElement ctime_select_static(const bool selection, const RingElement &a, const RingElement &b) {
        RingElement out;
        out.r_m1 = a.r_m1.ctime_select(selection, b.r_m1);
        out.r_m2 = a.r_m2.ctime_select(selection, b.r_m2);
        return out;
    }

    inline RingElement add(const RingElement &a, const RingElement &b) const {
        RingElement out;
        out.r_m1 = a.r_m1.add(b.r_m1).cmp_sub(multiplier.intrns2.m1.get_moduli());
        out.r_m2 = a.r_m2.add(b.r_m2).cmp_sub(multiplier.intrns2.m2.get_moduli());
        return out;
    }

    inline RingElement cmp_sub(const RingElement &a) const {
        RingElement out;
        out.r_m1 = a.r_m1.cmp_sub(multiplier.intrns2.m1.get_moduli());
        out.r_m2 = a.r_m2.cmp_sub(multiplier.intrns2.m2.get_moduli());
        return out;
    }

    inline RingElementWide cmp_sub(const RingElementWide &a) const {
        RingElementWide out;
        out.lo = cmp_sub(a.lo);
        out.hi = cmp_sub(a.hi);
        return out;
    }

    inline RingElementSmall add(const RingElementSmall &a, const RingElementSmall &b) const {
        RingElementSmall out;
        out.r_m2 = a.r_m2.add(b.r_m2).cmp_sub(multiplier.intrns2.m2.get_moduli());
        return out;
    }
    
    inline RingElementWide wide_add(const RingElementWide &a, const RingElementWide &b) const {
        RingElementWide out;
        out.lo = add(a.lo, b.lo);
        out.hi = add(a.hi, b.hi);
        return out;
    }

    inline RingElement sub(const RingElement &a, const RingElement &b) const {
        RingElement out;
        // For subtraction: (a - b) mod m
        // Strategy: subtract first, then if underflow occurred (b > a), add moduli
        // Use mask_add_cond to conditionally add moduli when b > a (underflow)
        out.r_m1 = a.r_m1.sub(b.r_m1);
        out.r_m1 = out.r_m1.mask_add_cond(b.r_m1, a.r_m1, multiplier.intrns2.m1.get_moduli());
        // Ensure result is less than moduli
        // out.r_m1 = out.r_m1.cmp_sub(multiplier.intrns2.m1.get_moduli());
        
        out.r_m2 = a.r_m2.sub(b.r_m2);
        out.r_m2 = out.r_m2.mask_add_cond(b.r_m2, a.r_m2, multiplier.intrns2.m2.get_moduli());
        // Ensure result is less than moduli
        // out.r_m2 = out.r_m2.cmp_sub(multiplier.intrns2.m2.get_moduli());
        return out;
    }

    inline RingElementSmall sub(const RingElementSmall &a, const RingElementSmall &b) const {
        RingElementSmall out;
        out.r_m2 = a.r_m2.sub(b.r_m2).mask_add_cond(b.r_m2, a.r_m2, multiplier.intrns2.m2.get_moduli());
        return out;
    }

    inline RingElementWide wide_sub(const RingElementWide &a, const RingElementWide &b) const {
        RingElementWide out;
        out.lo = sub(a.lo, b.lo);
        out.hi = sub(a.hi, b.hi);
        return out;
    }

    inline RingElementWide wide_mul(const RingElement &a, const RingElement &b) const {
        RingElementWide out;
        multiplier.intrns2.mul_wide(a.r_m1, a.r_m2, b.r_m1, b.r_m2, out.lo.r_m1, out.hi.r_m1, out.lo.r_m2, out.hi.r_m2);
        return out;
    }

    inline RingElement wide_reduce(const RingElementWide &a) const {
        RingElement out;
        multiplier.intrns2.reduce_wide(a.lo.r_m1, a.hi.r_m1, a.lo.r_m2, a.hi.r_m2, out.r_m1, out.r_m2);
        return out;
    }

    template<int batch>
    INLINE
    inline std::array<RingElementSmall, batch> batch_wide_reduce(const std::array<RingElementWide, batch> &a) const {
        std::array<RingElementSmall, batch> result;
        std::array<AVXVector<limbs>, batch> result_m2;
        std::array<AVXVector<limbs>, batch> a1_lo;
        std::array<AVXVector<limbs>, batch> a1_hi;
        std::array<AVXVector<limbs>, batch> a2_lo;
        std::array<AVXVector<limbs>, batch> a2_hi;
        for (int i = 0; i < batch; i++) {
            a1_lo[i] = a[i].lo.r_m1;
            a1_hi[i] = a[i].hi.r_m1;
            a2_lo[i] = a[i].lo.r_m2;
            a2_hi[i] = a[i].hi.r_m2;
        }
        multiplier.intrns2.template reduce_wide_batch<batch>(a1_lo, a1_hi, a2_lo, a2_hi, result_m2);
        for (int i = 0; i < batch; i++) {
            result[i].r_m2 = result_m2[i];
        }
        return result;
    }

    template<int batch>
    INLINE
    inline std::array<RingElement, batch> batch_expand(const std::array<RingElementSmall, batch> &a) const {
        std::array<RingElement, batch> result;
        std::array<AVXVector<limbs>, batch> result_m1;
        std::array<AVXVector<limbs>, batch> result_m2;
        for (int i = 0; i < batch; i++) {
            result_m2[i] = a[i].r_m2;
        }
        multiplier.intrns2.template batch_expand_wide<batch>(result_m2, result_m1);
        for (int i = 0; i < batch; i++) {
            result[i].r_m1 = result_m1[i];
            result[i].r_m2 = result_m2[i];
        }
        return result;
    }

    template<int batch> // This is used in batched multiplication
    INLINE
    inline std::array<RingElement, batch> batch_wide_reduce_expand(const std::array<RingElementWide, batch> &a) const {
        std::array<RingElement, batch> result;
        std::array<AVXVector<limbs>, batch> result_m1;
        std::array<AVXVector<limbs>, batch> result_m2;
        std::array<AVXVector<limbs>, batch> a1_lo;
        std::array<AVXVector<limbs>, batch> a1_hi;
        std::array<AVXVector<limbs>, batch> a2_lo;
        std::array<AVXVector<limbs>, batch> a2_hi;
        for (int i = 0; i < batch; i++) {
            a1_lo[i] = a[i].lo.r_m1;
            a1_hi[i] = a[i].hi.r_m1;
            a2_lo[i] = a[i].lo.r_m2;
            a2_hi[i] = a[i].hi.r_m2;
        }
        multiplier.intrns2.template reduce_wide_batch<batch>(a1_lo, a1_hi, a2_lo, a2_hi, result_m2);
        multiplier.intrns2.template batch_expand_wide<batch>(result_m2, result_m1);
        for (int i = 0; i < batch; i++) {
            result[i].r_m1 = result_m1[i];
            result[i].r_m2 = result_m2[i];
        }
        return result;
    }

    template<int batch, bool serialize_reduce = false> // This is used in batched multiplication
    INLINE
    static inline std::array<RingElement, batch> true_batch_wide_reduce_expand(const std::array<const Ring<modulus_bits, num_additions> *, batch> &these, const std::array<RingElementWide, batch> &a) {
        std::array<RingElement, batch> result;
        std::array<AVXVector<limbs>, batch> result_m1;
        std::array<AVXVector<limbs>, batch> result_m2;
        std::array<AVXVector<limbs>, batch> a1_lo;
        std::array<AVXVector<limbs>, batch> a1_hi;
        std::array<AVXVector<limbs>, batch> a2_lo;
        std::array<AVXVector<limbs>, batch> a2_hi;
        for (int i = 0; i < batch; i++) {
            a1_lo[i] = a[i].lo.r_m1;
            a1_hi[i] = a[i].hi.r_m1;
            a2_lo[i] = a[i].lo.r_m2;
            a2_hi[i] = a[i].hi.r_m2;
        }
        std::array<const IntRNS2<MontgomeryReduce, limbs, limbs> *, batch> these_intrns2;
        for (int i = 0; i < batch; i++) {
            these_intrns2[i] = &(these[i]->multiplier.intrns2);
        }
        if constexpr (serialize_reduce) {
            for (int i = 0; i < batch; i++) {
                these_intrns2[i]->reduce_wide_out2_only(a1_lo[i], a1_hi[i], a2_lo[i], a2_hi[i], result_m2[i]);
            }
        } else {
            IntRNS2<MontgomeryReduce, limbs, limbs>::template reduce_wide_true_batch<batch>(these_intrns2, a1_lo, a1_hi, a2_lo, a2_hi, result_m2);
        }
        // IntRNS2<MontgomeryReduce, limbs, limbs>::template true_batch_expand_wide<batch>(these_intrns2, result_m2, result_m1);
        these_intrns2[0]->template batch_expand_wide<batch>(result_m2, result_m1); // Ok since the second CRNS matrix is modulus independent
        for (int i = 0; i < batch; i++) {
            result[i].r_m1 = result_m1[i];
            result[i].r_m2 = result_m2[i];
        }
        return result;
    }

    // template<int batch>
    // inline std::array<RingElementSmall, batch> batch_mult_reduce(const std::array<RingElement, batch> &a, const std::array<RingElement, batch> &b) const {
    //     std::array<RingElementWide, batch> result_wide;
    //     for (int i = 0; i < batch; i++) {
    //         result_wide[i] = wide_mul(a[i], b[i]);
    //     }
    //     return batch_wide_reduce<batch>(result_wide);
    // }

    template<int batch>  // This is used in batched multiplication
    INLINE
    inline std::array<RingElement, batch> batch_mulreduce(const std::array<RingElement, batch> &a, const std::array<RingElement, batch> &b) const {
        std::array<RingElementWide, batch> result_wide;
        for (int i = 0; i < batch; i++) {
            result_wide[i] = wide_mul(a[i], b[i]);
        }
        return batch_wide_reduce_expand<batch>(result_wide);
    }

    template<int batch, bool serialize_reduce = false>  // This is used in batched multiplication
    INLINE
    static inline std::array<RingElement, batch> true_batch_mulreduce(const std::array<const Ring<modulus_bits, num_additions> *, batch> &these, const std::array<RingElement, batch> &a, const std::array<RingElement, batch> &b) {
        std::array<RingElementWide, batch> result_wide;
        for (int i = 0; i < batch; i++) {
            result_wide[i] = these[0]->wide_mul(a[i], b[i]); // TODO: can we further batch the operations inside?
        }
        return Ring<modulus_bits, num_additions>::template true_batch_wide_reduce_expand<batch, serialize_reduce>(these, result_wide);
    }

    template<int batch>
    INLINE
    inline std::array<RingElementWide, batch> batch_mul_acc(const std::array<RingElement, batch> &a, const std::array<RingElement, batch> &b) const {
        // Initialize result to zero
        std::array<RingElementWide, batch> result_wide{};
        for (int i = 0; i < batch; i++) {
            multiplier.intrns2.mul_acc_wide(a[i].r_m1, a[i].r_m2, b[i].r_m1, b[i].r_m2, result_wide[i].lo.r_m1, result_wide[i].hi.r_m1, result_wide[i].lo.r_m2, result_wide[i].hi.r_m2);
            // TODO: reduce the number of comparisons by subtracting larger multiples of the modulus after several additions
            if (i > 0) {
                result_wide[i] = cmp_sub(result_wide[i]);
            }
        }
        return result_wide;
    }
};
