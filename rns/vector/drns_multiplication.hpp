#pragma once
#include "changebase.hpp"
#include "vector_impl.hpp"
#include "../precompute/precompute.hpp"
#include "../reduction/montgomery.hpp"
#include "conversion.hpp"
#include <cstdio>
#include <cstdlib>

#include "multiplication_common.hpp"

template<bool batch_emult = false, bool sumprod = false>
struct DRNSVariant {
    constexpr static bool batch_emult_ = batch_emult;
    constexpr static bool sumprod_ = sumprod;
};

template<template <int> class Reduction, int limbs1, int limbs2>
class DRNS {
    
    public:

    const RNSMatrix<limbs1, limbs2> r1;
    const RNSMatrix<limbs2, limbs1> r2;
    const Reduction<limbs1> m1;
    const Reduction<limbs2> m2;

    // Constructor building precompute with separate premult/postmult for each matrix
    inline DRNS(
        const std::array<uint64_t, limbs1> &moduli_in_m1,
        const std::array<uint64_t, limbs2> &moduli_in_m2,
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
    }

    template<typename Variant>
    INLINE
    inline void mul_reduce(
        const AVXVector<limbs1> &mont_factor_rns,
        const AVXVector<limbs2> &p_rns,
        const AVXVector<limbs1> &a1, // a_i'
        const AVXVector<limbs2> &a2, // a_j'
        const AVXVector<limbs1> &b1, // b_i'
        const AVXVector<limbs2> &b2, // b_j'
        AVXVector<limbs1> &out1, // r_i
        AVXVector<limbs2> &out2) const // r_j
    {
        // std::cerr << "DRNS::mul_reduce called" << std::endl;
        // https://eprint.iacr.org/2025/1068.pdf Table 1, variant of row "DRNS Simple"
        // See make_DRNSSystemMontgomery for the values of premult1, postmult1, premult2, postmult2

        // s_i = a_i' * b_i' mod M
        auto ab_m1 = ele_mult(a1, b1, m1);
        AVXVector<limbs1> abd_m1;
        AVXVector<limbs2> ab_m2;
        if constexpr (Variant::sumprod_) { // 3 full reductions and 1 small reduction
            // std::cerr << "sumprod" << std::endl;
            // s_i * d mod M
            abd_m1 = ele_mult(ab_m1, mont_factor_rns, m1); // DIFFERENCE from "Opt Mont": move premultiplication by d=p^{-1} mod M outside of the CRNS matrix r1
            // q_j = CRNS^{M * premult1}_{N * postmult1} (s_i * d)
            out2 = r1.rns_reduce(abd_m1, m2);
            // r_j = a_j' * b_j' + q_j * p mod N
            // out2 = out2.add(ab_m2).cmp_sub(m2.get_moduli()); // DIFFERENCE from "Opt Mont": move postmultiplication by p mod N outside of the CRNS matrix r1
            out2 = ele_sum_prod(a2, b2, out2, p_rns, m2);
            // r_i = CRNS^{N * premult2}_{M * postmult2} (r_j)
            out1 = r2.rns_reduce(out2, m1);
        } else { // 2 full reductions, 3 small reductions, and 1 conditional subtraction
            if constexpr (!Variant::batch_emult_) {
                // std::cerr << "regular" << std::endl;
                // s_i * d mod M
                abd_m1 = ele_mult(ab_m1, mont_factor_rns, m1); // DIFFERENCE from "Opt Mont": move premultiplication by d=p^{-1} mod M outside of the CRNS matrix r1
                // s_j = a_j' * b_j' mod N
                ab_m2 = ele_mult(a2, b2, m2);
            } else {
                // std::cerr << "batch_emult" << std::endl;
                // s_i * d mod M
                // s_j = a_j' * b_j' mod N
                std::array<AVXVector<limbs1>, 2> ab_m1_a2{ab_m1, a2};
                std::array<AVXVector<limbs2>, 2> d_b2{mont_factor_rns, b2};
                std::array<MontgomeryReduce<limbs1>, 2> moduli{m1, m2};
                auto [abd_m1_, ab_m2_] = batch_ele_mult(ab_m1_a2, d_b2, moduli); // [s_i * d, a_j' * b_j']
                abd_m1 = abd_m1_;
                ab_m2 = ab_m2_;
            }
            // q_j = CRNS^{M * premult1}_{N * postmult1} (s_i * d)
            out2 = r1.rns_reduce(abd_m1, m2);
            // q_j * p mod N
            out2 = ele_mult(out2, p_rns, m2);
            // r_j = s_j + q_j * p mod N
            out2 = out2.add(ab_m2).cmp_sub(m2.get_moduli()); // DIFFERENCE from "Opt Mont": move postmultiplication by p mod N outside of the CRNS matrix r1
            // r_i = CRNS^{N * premult2}_{M * postmult2} (r_j)
            out1 = r2.rns_reduce(out2, m1);
        }
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

    template<int batch>
    INLINE
    inline void reduce_wide_true_batch(
        const std::array<AVXVector<limbs1>, batch> &mont_factor_rns, // reduce_wide_batch would have mont_factor_rns and p_rns be the same for all rings
        const std::array<AVXVector<limbs2>, batch> &p_rns,
        const std::array<AVXVector<limbs1>, batch> &a1_lo, const std::array<AVXVector<limbs1>, batch> &a1_hi, const std::array<AVXVector<limbs2>, batch> &a2_lo, const std::array<AVXVector<limbs2>, batch> &a2_hi, std::array<AVXVector<limbs2>, batch> &out2) const {
        std::array<AVXVector<limbs1>, batch> a1;
        std::array<AVXVector<limbs1>, batch> a1_p_lo;
        std::array<AVXVector<limbs1>, batch> a1_p_hi;
        std::array<AVXVector<limbs2>, batch> q_hi;
        std::array<AVXVector<limbs2>, batch> q_lo;
        std::array<AVXVector<limbs2>, batch> q_mod_n;
        std::array<AVXVector<limbs2>, batch> ab_qp_mod_n_lo;
        std::array<AVXVector<limbs2>, batch> ab_qp_mod_n_hi;
        // std::array<AVXVector<limbs2>, batch> a2_lo;
        // std::array<AVXVector<limbs2>, batch> a2_hi;

        a1 = m1.template reduce_small_batch<batch>(a1_hi, a1_lo);
        for (int i = 0; i < batch; i++) {
            a1_p_lo[i] = AVXVector<limbs1>(0).mullo(a1[i], mont_factor_rns[i]);
            a1_p_hi[i] = AVXVector<limbs1>(0).mulhi(a1[i], mont_factor_rns[i]);
        }
        a1 = m1.template reduce_small_batch<batch>(a1_p_hi, a1_p_lo);
        for (int i = 0; i < batch; i++) {
            q_hi[i] = AVXVector<limbs2>(0);
            q_lo[i] = AVXVector<limbs2>(0);
        }
        r1.template rns_reduce_raw_batch<batch>(a1, q_hi, q_lo);
        q_mod_n = m2.template reduce_full_batch<batch>(q_hi, q_lo);
        // The following is like ele_sum_prod in the unbatched version. TODO: implement other variants.
        for (int i = 0; i < batch; i++) {
            ab_qp_mod_n_lo[i] = a2_lo[i].mullo(q_mod_n[i], p_rns[i]);
            ab_qp_mod_n_hi[i] = a2_hi[i].mulhi(q_mod_n[i], p_rns[i]);
        }
        out2 = m2.template reduce_full_batch<batch>(ab_qp_mod_n_hi, ab_qp_mod_n_lo);
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
};

template<int bits, int limbs>
struct DRNSSystem {
    DRNS<MontgomeryReduce, limbs, limbs> intrns2;
    ConvertToRNS<bits, limbs> convert_to;
    ConvertFromRNS<bits, limbs> convert_from;
    std::array<uint64_t, limbs> moduli1;
    std::array<uint64_t, limbs> moduli2;
    AVXVector<limbs> mont_factor_rns; // (-p^{-1} * 2^w) mod M
    AVXVector<limbs> p_rns; // p * 2^w mod N

    // Convert a BigInt to Montgomery form in RNS representation
    // Returns (a_m1, a_m2) where a_m1 is in moduli1 and a_m2 is in moduli2
    inline std::pair<AVXVector<limbs>, AVXVector<limbs>> to_mont_avx(const BigInt &a) const {
        AVXVector<limbs> a_m2 = convert_to.convert_to_rns(a, intrns2.m2);
        AVXVector<limbs> a_m1 = intrns2.r2.rns_reduce(a_m2, intrns2.m1);
        return std::make_pair(a_m1, a_m2);
    }

    // Convert from Montgomery form in RNS representation back to BigInt
    // Takes a_m2 (representation in moduli2) and returns the BigInt value
    inline BigInt from_mont_avx(const AVXVector<limbs> &a_m2) const {
        return convert_from.convert_from_rns(a_m2);
    }

    // Multiply and reduce in RNS representation
    // Takes a_m1, a_m2, b_m1, b_m2 and computes (a*b)_m1, (a*b)_m2
    template<typename Variant>
    inline void mul_reduce(
        const AVXVector<limbs> &a_m1,
        const AVXVector<limbs> &a_m2,
        const AVXVector<limbs> &b_m1,
        const AVXVector<limbs> &b_m2,
        AVXVector<limbs> &out_m1,
        AVXVector<limbs> &out_m2
    ) const {
        intrns2.template mul_reduce<Variant>(mont_factor_rns, p_rns, a_m1, a_m2, b_m1, b_m2, out_m1, out_m2);
    }

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

// Factory producing DRNS plus compatible converters following python_reduction.DRNS
// https://eprint.iacr.org/2025/1068.pdf Table 1, variant of row "DRNS Simple"
template<int bits, int limbs>
inline DRNSSystem<bits, limbs> make_DRNSSystemMontgomery(
    const BigInt &target, // p
    const int modbits = 52,
    const int max_bound = 9
) {
    auto moduli = make_RNSModuli<limbs>(target);
    const BigInt m1 = moduli.M.modulus; // M
    const BigInt m2 = moduli.N.modulus; // N

    // std::cerr << "make_DRNSSystemMontgomery called" << std::endl;
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

    const BigInt premult1 = r_inv_m1; // premult1 = 2^{-w} mod M
    const BigInt postmult1 = (inv_factor * inv_factor % m2 * r % m2 * r) % m2; // postmult1 = M^{-2} * 2^{2w} mod N

    const BigInt r_inv_m2 = r.mod_inverse(m2); // 2^{-w} mod N

    const BigInt premult2 = (m1 % m2) * r_inv_m2 % m2; // premult2 = M * 2^{-w} mod N
    const BigInt postmult2 = (r % m1) * (r % m1) % m1; // postmult2 = 2^{2w} mod M

    const BigInt m1_inv_target = m1.mod_inverse(target); // M^{-1} mod p
    const BigInt conv_from_postmult = (m1_inv_target << 64) % target;

    auto to_rns = ConvertToRNS<bits, limbs>(
        m1, // premult_convert_to = M
        (inv_factor % m2) * (r % m2) % m2 * (r % m2) % m2, // postmult_convert_to = M^{-1} * 2^{2w} mod N
        target,
        moduli2
    );

    std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> mont_factor_rns_data;
    mont_factor_rns_data.fill(0); // Fill with zeros to prevent uninitialized values
    for (int i = 0; i < limbs; i++) {
        mont_factor_rns_data[i] = static_cast<uint64_t>((mont_factor * r % moduli1[i]).to_ulong());
    }
    AVXVector<limbs> mont_factor_rns(0);
    mont_factor_rns.load(mont_factor_rns_data.data());

    std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> p_rns_data;
    p_rns_data.fill(0); // Fill with zeros to prevent uninitialized values
    for (int i = 0; i < limbs; i++) {
        p_rns_data[i] = static_cast<uint64_t>((target * r % moduli2[i]).to_ulong());
    }
    AVXVector<limbs> p_rns(0);
    p_rns.load(p_rns_data.data());

    DRNSSystem<bits, limbs> sys{
        DRNS<MontgomeryReduce, limbs, limbs>(moduli1, moduli2, premult1, postmult1, premult2, postmult2),
        // For ConvertToRNS and ConvertFromRNS, see Appendix D.1
        to_rns,
        ConvertFromRNS<bits, limbs>(
            (m1 % m2) * r_inv_m2 % m2, // premult_convert_from = M * 2^{-w} mod N
            conv_from_postmult, // postmult_convert_from = M^{-1} * 2^{64} mod p
            target,
            moduli2
        ),
        moduli1,
        moduli2,
        mont_factor_rns,
        p_rns
    };
    return sys;
}

