#pragma once
#include "changebase.hpp"
#include "vector_impl.hpp"
#include "../precompute/precompute.hpp"
#include "../reduction/montgomery.hpp"
#include "conversion.hpp"
#include <cstdio>
#include <cstdlib>

#include "multiplication_common.hpp"

template<template <int> class Reduction, int limbs1, int limbs2>
class IntRNS2 {
    
    public:

    const RNSMatrix<limbs1, limbs2> r1;
    const RNSMatrix<limbs2, limbs1> r2;
    const Reduction<limbs1> m1;
    const Reduction<limbs2> m2;

    // Constructor building precompute with separate premult/postmult for each matrix
    inline IntRNS2(
        const std::array<uint64_t, limbs1> &moduli_in_m1,
        const std::array<uint64_t, limbs2> &moduli_in_m2,
        const BigInt &target_modulus,
        const BigInt &premult1,
        const BigInt &postmult1,
        const BigInt &premult2,
        const BigInt &postmult2
    )
    : r1(moduli_in_m1, moduli_in_m2, BigInt(0), premult1, postmult1)  // target_modulus=0 means use modulus_out (matching Python None)
    , r2(moduli_in_m2, moduli_in_m1, BigInt(0), premult2, postmult2)  // target_modulus=0 means use modulus_out (matching Python None)
    , m1(moduli_in_m1)
    , m2(moduli_in_m2)
    {
        (void)target_modulus;
    }

    INLINE
    inline void mul_reduce(
        const AVXVector<limbs1> &a1, // a_i'
        const AVXVector<limbs2> &a2, // a_j'
        const AVXVector<limbs1> &b1, // b_i'
        const AVXVector<limbs2> &b2, // b_j'
        AVXVector<limbs1> &out1, // r_i
        AVXVector<limbs2> &out2) const // r_j
    {
        // std::cerr << "IntRNS2::mul_reduce called" << std::endl;
        // https://eprint.iacr.org/2025/1068.pdf Table 1, row "Opt Mont"
        // See make_IntRNS2SystemMontgomery for the values of premult1, postmult1, premult2, postmult2

        // s_i = a_i' * b_i' mod M
        auto ab_m1 = ele_mult(a1, b1, m1);
        // q_j = CRNS^{M * premult1}_{N * postmult1} (s_i)
        // r_j = a_j' * b_j' + q_j mod N
        out2 = r1.rns_reduce_with_acc(ab_m1, a2, b2, m2);
        // r_i = CRNS^{N * premult2}_{M * postmult2} (r_j)
        out1 = r2.rns_reduce(out2, m1);
    }

    // Alternative implementation of mul_reduce that appears to work. Unsure how much slower it is.
    inline void mul_reduce_alt(
        const AVXVector<limbs1> &a1, // a_i'
        const AVXVector<limbs2> &a2, // a_j'
        const AVXVector<limbs1> &b1, // b_i'
        const AVXVector<limbs2> &b2, // b_j'
        AVXVector<limbs1> &out1, // r_i
        AVXVector<limbs2> &out2) const // r_j
    {
        // https://eprint.iacr.org/2025/1068.pdf Table 1, row "Opt Mont"
        // See make_IntRNS2SystemMontgomery for the values of premult1, postmult1, premult2, postmult2

        // s_i = a_i' * b_i' mod M
        auto ab_m1 = ele_mult(a1, b1, m1);
        // s_j = a_j' * b_j' mod N
        auto ab_m2 = ele_mult(a2, b2, m2);
        // q_j = CRNS^{M * premult1}_{N * postmult1} (s_i)
        out2 = r1.rns_reduce(ab_m1, m2);
        // r_j = s_j + q_j mod N
        out2 = out2.add(ab_m2).cmp_sub(m2.get_moduli());
        // r_i = CRNS^{N * premult2}_{M * postmult2} (r_j)
        out1 = r2.rns_reduce(out2, m1);
    }

    INLINE
    inline void mul_wide(const AVXVector<limbs1> &a1, const AVXVector<limbs2> &a2, const AVXVector<limbs1> &b1, const AVXVector<limbs2> &b2, AVXVector<limbs1> &out1_lo, AVXVector<limbs1> &out1_hi, AVXVector<limbs2> &out2_lo, AVXVector<limbs2> &out2_hi) const {
        out1_lo = AVXVector<limbs1>(0).mullo(a1, b1);
        out1_hi = AVXVector<limbs1>(0).mulhi(a1, b1);
        out2_lo = AVXVector<limbs2>(0).mullo(a2, b2);
        out2_hi = AVXVector<limbs2>(0).mulhi(a2, b2);
    }

    INLINE
    inline void mul_acc_wide(const AVXVector<limbs1> &a1, const AVXVector<limbs2> &a2, const AVXVector<limbs1> &b1, const AVXVector<limbs2> &b2, AVXVector<limbs1> &out1_lo, AVXVector<limbs1> &out1_hi, AVXVector<limbs2> &out2_lo, AVXVector<limbs2> &out2_hi) const {
        out1_lo = out1_lo.mullo(a1, b1);
        out1_hi = out1_hi.mulhi(a1, b1);
        out2_lo = out2_lo.mullo(a2, b2);
        out2_hi = out2_hi.mulhi(a2, b2);
    }

    INLINE
    inline void reduce_wide(const AVXVector<limbs1> &a1_lo, const AVXVector<limbs1> &a1_hi, const AVXVector<limbs2> &a2_lo, const AVXVector<limbs2> &a2_hi, AVXVector<limbs1> &out1, AVXVector<limbs2> &out2) const {
        auto a1 = m1.reduce_small(a1_hi, a1_lo);
        // need to make copy of a2_hi and a2_lo to avoid modifying the input
        auto a2_hi_copy = a2_hi;
        auto a2_lo_copy = a2_lo;
        r1.rns_reduce_raw(a1, a2_hi_copy, a2_lo_copy);
        out2 = m2.reduce_full(a2_hi_copy, a2_lo_copy);
        out1 = r2.rns_reduce(out2, m1);
    }

    // Like reduce_wide_batch<1> but without the clutter
    INLINE
    inline void reduce_wide_out2_only(const AVXVector<limbs1> &a1_lo, const AVXVector<limbs1> &a1_hi, const AVXVector<limbs2> &a2_lo, const AVXVector<limbs2> &a2_hi, AVXVector<limbs2> &out2) const {
        auto a1 = m1.reduce_small(a1_hi, a1_lo);
        // need to make copy of a2_hi and a2_lo to avoid modifying the input
        auto a2_hi_copy = a2_hi;
        auto a2_lo_copy = a2_lo;
        r1.rns_reduce_raw(a1, a2_hi_copy, a2_lo_copy);
        out2 = m2.reduce_full(a2_hi_copy, a2_lo_copy);
    }

    template<int batch>
    INLINE
    inline void reduce_wide_batch(const std::array<AVXVector<limbs1>, batch> &a1_lo, const std::array<AVXVector<limbs1>, batch> &a1_hi, const std::array<AVXVector<limbs2>, batch> &a2_lo, const std::array<AVXVector<limbs2>, batch> &a2_hi, std::array<AVXVector<limbs2>, batch> &out2) const {
        std::array<AVXVector<limbs1>, batch> a1;
        std::array<AVXVector<limbs2>, batch> a2_hi_copy;
        std::array<AVXVector<limbs2>, batch> a2_lo_copy;
        a1 = m1.template reduce_small_batch<batch>(a1_hi, a1_lo);
        for (int i = 0; i < batch; i++) {
            a2_hi_copy[i] = a2_hi[i];
            a2_lo_copy[i] = a2_lo[i];
        }
        r1.template rns_reduce_raw_batch<batch>(a1, a2_hi_copy, a2_lo_copy);
        out2 = m2.template reduce_full_batch<batch>(a2_hi_copy, a2_lo_copy);
    }

    template<int batch>
    INLINE
    static inline void reduce_wide_true_batch(const std::array<const IntRNS2<Reduction, limbs1, limbs2> *, batch> &these, const std::array<AVXVector<limbs1>, batch> &a1_lo, const std::array<AVXVector<limbs1>, batch> &a1_hi, const std::array<AVXVector<limbs2>, batch> &a2_lo, const std::array<AVXVector<limbs2>, batch> &a2_hi, std::array<AVXVector<limbs2>, batch> &out2) {
        std::array<AVXVector<limbs1>, batch> a1;
        std::array<AVXVector<limbs2>, batch> a2_hi_copy;
        std::array<AVXVector<limbs2>, batch> a2_lo_copy;

        a1 = these[0]->m1.template reduce_small_batch<batch>(a1_hi, a1_lo); // assume M is the same for all rings

        for (int i = 0; i < batch; i++) {
            a2_hi_copy[i] = a2_hi[i];
            a2_lo_copy[i] = a2_lo[i];
        }

        std::array<const RNSMatrix<limbs1, limbs2> *, batch> these_r1;
        for (int i = 0; i < batch; i++) {
            these_r1[i] = &(these[i]->r1);
        }
        RNSMatrix<limbs1, limbs2>::template rns_reduce_raw_true_batch<batch>(these_r1, a1, a2_hi_copy, a2_lo_copy);

        out2 = these[0]->m2.template reduce_full_batch<batch>(a2_hi_copy, a2_lo_copy); // assume N is the same for all rings
    }

    template<int batch>
    INLINE
    inline void batch_expand_wide(const std::array<AVXVector<limbs2>, batch> &out2, std::array<AVXVector<limbs1>, batch> &out1) const {
        std::array<AVXVector<limbs1>, batch> z_hi;
        std::array<AVXVector<limbs1>, batch> z_lo;
        for (int i = 0; i < batch; i++) {
            z_hi[i] = AVXVector<limbs1>(0);
            z_lo[i] = AVXVector<limbs1>(0);
        }
        r2.template rns_reduce_raw_batch<batch>(out2, z_hi, z_lo);
        // Do I also need reduce_full_batch to amortize loading of Montgomery factors?
        out1 = m1.template reduce_full_batch<batch>(z_hi, z_lo);
    }

    template<int batch>
    INLINE
    static inline void true_batch_expand_wide(const std::array<const IntRNS2<Reduction, limbs1, limbs2> *, batch> &these, const std::array<AVXVector<limbs2>, batch> &out2, std::array<AVXVector<limbs1>, batch> &out1) {
        std::array<AVXVector<limbs1>, batch> z_hi;
        std::array<AVXVector<limbs1>, batch> z_lo;
        for (int i = 0; i < batch; i++) {
            z_hi[i] = AVXVector<limbs1>(0);
            z_lo[i] = AVXVector<limbs1>(0);
        }
        std::array<const RNSMatrix<limbs1, limbs2> *, batch> these_r2;
        for (int i = 0; i < batch; i++) {
            these_r2[i] = &(these[i]->r2);
        }
        RNSMatrix<limbs1, limbs2>::template rns_reduce_raw_true_batch<batch>(these_r2, out2, z_hi, z_lo);
        // // Do I also need reduce_full_batch to amortize loading of Montgomery factors?
        // std::array<Reduction<limbs1>, batch> these_m1;
        // for (int i = 0; i < batch; i++) {
        //     these_m1[i] = these[i].m1;
        // }
        // out1 = Reduction<limbs1>::template reduce_full_batch_diff_moduli<batch>(these_m1, z_hi, z_lo);
        out1 = these[0]->m1.template reduce_full_batch<batch>(z_hi, z_lo); // assume M is the same for all rings
    }
};

// Factory for Montgomery variant (single template parameter: limbs). Assumes limbs1==limbs2==limbs.
// Generates moduli using GenRNS; aborts if target cannot be reached with the given limb count.
template<int limbs>
inline IntRNS2<MontgomeryReduce, limbs, limbs> make_IntRNS2Montgomery(
    const BigInt &target,
    const int modbits = 52,
    const int max_bound = 9
) {
    // Generate first set of moduli covering target * max_bound
    GenRNS<limbs> g1(target * BigInt(max_bound), -1, BigInt(1), modbits);
    // Generate second set covering ~6*target, chaining state and previous modulus product
    GenRNS<limbs> g2(target * BigInt(6), g1.state, g1.modulus, modbits);

    const BigInt m1 = g1.modulus;
    const BigInt m2 = g2.modulus;
    if (!(m2 > BigInt(6) * target)) {
        fprintf(stderr, "Error: Insufficient limbs for gen RNS: m2 <= 6*target\n");
        abort();
    }

    std::array<uint64_t, limbs> moduli1 = g1.moduli;
    std::array<uint64_t, limbs> moduli2 = g2.moduli;

    // Montgomery parameters following python_reduction.IntRNS2
    constexpr int mulbits = 52;
    const BigInt r = BigInt(1) << mulbits;

    // r1 parameters
    const BigInt mont_factor = (BigInt(0) - target).mod_inverse(m1); // pow(-target, -1, m1)
    const BigInt r_inv_m1 = r.mod_inverse(m1);
    const BigInt premult1 = (mont_factor * r_inv_m1) % m1;

    const BigInt inv_factor = m1.mod_inverse(m2); // m1^{-1} mod m2
    const BigInt postmult1 = (target * inv_factor % m2) * (inv_factor % m2) % m2 * (r % m2) % m2 * (r % m2) % m2;

    // r2 parameters
    const BigInt r_inv_m2 = r.mod_inverse(m2);
    const BigInt premult2 = (m1 % m2) * r_inv_m2 % m2;
    const BigInt postmult2 = (r % m1) * (r % m1) % m1; // but postmult is used under modulus_out (m1) inside precompute

    return IntRNS2<MontgomeryReduce, limbs, limbs>(moduli1, moduli2, target, premult1, postmult1, premult2, postmult2);
}

template<int bits, int limbs>
struct IntRNS2System {
    IntRNS2<MontgomeryReduce, limbs, limbs> intrns2;
    ConvertToRNS<bits, limbs> convert_to;
    ConvertFromRNS<bits, limbs> convert_from;
    std::array<uint64_t, limbs> moduli1;
    std::array<uint64_t, limbs> moduli2;

    // Convert a BigInt to Montgomery form in RNS representation
    // Returns (a_m1, a_m2) where a_m1 is in moduli1 and a_m2 is in moduli2
    INLINE
    inline std::pair<AVXVector<limbs>, AVXVector<limbs>> to_mont_avx(const BigInt &a) const {
        AVXVector<limbs> a_m2 = convert_to.convert_to_rns(a, intrns2.m2);
        AVXVector<limbs> a_m1 = intrns2.r2.rns_reduce(a_m2, intrns2.m1);
        return std::make_pair(a_m1, a_m2);
    }

    // Convert from Montgomery form in RNS representation back to BigInt
    // Takes a_m2 (representation in moduli2) and returns the BigInt value
    INLINE
    inline BigInt from_mont_avx(const AVXVector<limbs> &a_m2) const {
        return convert_from.convert_from_rns(a_m2);
    }

    // Multiply and reduce in RNS representation
    // Takes a_m1, a_m2, b_m1, b_m2 and computes (a*b)_m1, (a*b)_m2
    INLINE
    inline void mul_reduce(
        const AVXVector<limbs> &a_m1,
        const AVXVector<limbs> &a_m2,
        const AVXVector<limbs> &b_m1,
        const AVXVector<limbs> &b_m2,
        AVXVector<limbs> &out_m1,
        AVXVector<limbs> &out_m2
    ) const {
        intrns2.mul_reduce(a_m1, a_m2, b_m1, b_m2, out_m1, out_m2);
    }

    INLINE
    inline void raw_mul(
        const AVXVector<limbs> &a_m1,
        const AVXVector<limbs> &a_m2,
        const AVXVector<limbs> &b_m1,
        const AVXVector<limbs> &b_m2,
        AVXVector<limbs> &out_m1,
        AVXVector<limbs> &out_m2
    ) const {
        intrns2.raw_mul(a_m1, a_m2, b_m1, b_m2, out_m1, out_m2);
    }
};

// Factory producing IntRNS2 plus compatible converters following python_reduction.IntRNS2
// https://eprint.iacr.org/2025/1068.pdf Table 1, row "Opt Mont"
template<int bits, int limbs>
inline IntRNS2System<bits, limbs> make_IntRNS2SystemMontgomery(
    const BigInt &target, // p
    const int modbits = 52,
    const int max_bound = 9
) {
    auto moduli = make_RNSModuli<limbs>(target);
    const BigInt m1 = moduli.M.modulus; // M
    const BigInt m2 = moduli.N.modulus; // N

    // std::cerr << "make_IntRNS2SystemMontgomery called" << std::endl;
    if (!(m2 > BigInt(6) * target)) {
        fprintf(stderr, "Error: Insufficient limbs for gen RNS: m2 <= 6*target\n");
        abort();
    }
    std::array<uint64_t, limbs> moduli1;
    std::array<uint64_t, limbs> moduli2;
    for (int i = 0; i < limbs; i++) {
        moduli1[i] = moduli.M.moduli[i];
        moduli2[i] = moduli.N.moduli[i];
    }

    constexpr int mulbits = 52;
    const BigInt r = BigInt(1) << mulbits; // 2^w

    const BigInt mont_factor = (BigInt(0) - target).mod_inverse(m1); // d = -p^{-1} mod M
    const BigInt r_inv_m1 = r.mod_inverse(m1); // 2^{-w} mod M
    const BigInt inv_factor = m1.mod_inverse(m2); // M^{-1} mod N

    const BigInt premult1 = (mont_factor * r_inv_m1) % m1; // premult1 = d * 2^{-w} mod M
    const BigInt postmult1 = ((target * inv_factor) % m2 * inv_factor % m2 * r % m2 * r) % m2; // postmult1 = p * M^{-2} * 2^{2w} mod N

    const BigInt r_inv_m2 = r.mod_inverse(m2); // 2^{-w} mod N

    const BigInt premult2 = (m1 % m2) * r_inv_m2 % m2; // premult2 = M * 2^{-w} mod N
    const BigInt postmult2 = (r % m1) * (r % m1) % m1; // postmult2 = 2^{2w} mod M

    const BigInt m1_inv_target = m1.mod_inverse(target); // M^{-1} mod p
    const BigInt conv_from_postmult = (m1_inv_target << 64) % target;

    IntRNS2System<bits, limbs> sys{
        IntRNS2<MontgomeryReduce, limbs, limbs>(moduli1, moduli2, target, premult1, postmult1, premult2, postmult2),
        // For ConvertToRNS and ConvertFromRNS, see Appendix D.1
        ConvertToRNS<bits, limbs>(
            m1, // premult_convert_to = M
            (inv_factor % m2) * (r % m2) % m2 * (r % m2) % m2, // postmult_convert_to = M^{-1} * 2^{2w} mod N
            target,
            moduli2
        ),
        ConvertFromRNS<bits, limbs>(
            (m1 % m2) * r_inv_m2 % m2, // premult_convert_from = M * 2^{-w} mod N
            conv_from_postmult, // postmult_convert_from = M^{-1} * 2^{64} mod p
            target,
            moduli2
        ),
        moduli1,
        moduli2
    };
    return sys;
}

