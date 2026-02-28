#pragma once
#include "../crypto/common.hpp"
#include "vector_impl.hpp"
#include "../precompute/precompute.hpp"
#include <type_traits>
#include <iostream>
typedef unsigned __int128 uint128_t;

template<bool unroll2 = false>
struct RNSVariant {
    constexpr static bool unroll2_ = unroll2;
};

template <int in_limbs, int out_limbs, int IN_DIGITS=1, int OUT_DIGITS=1>
class RNSMatrix {

    private:
        AVXVector<out_limbs*OUT_DIGITS> rns_mat[in_limbs*IN_DIGITS];
        uint64_t shifted_quotient_estimations[in_limbs*IN_DIGITS];
        AVXVector<out_limbs*OUT_DIGITS> correction;
        BigInt premult;
        BigInt postmult;

        static constexpr int precision = 64;
        static constexpr int mulbits = 52;

    public:

    // Helper to convert uint64_t array to BigInt array
    template<typename T, size_t N>
    static inline std::array<BigInt, N> to_bigint_array(const std::array<T, N> &arr) {
        if constexpr (std::is_same_v<T, BigInt>) {
            return arr; // No conversion needed
        } else {
            std::array<BigInt, N> result;
            for (size_t i = 0; i < N; i++) {
                result[i] = BigInt(arr[i]);
            }
            return result;
        }
    }

    // Template constructor that accepts both BigInt and uint64_t moduli arrays
    template<int INBITS, int OUTBITS, typename ModuliInT, typename ModuliOutT>
    inline RNSMatrix(
        const std::array<ModuliInT, in_limbs> &moduli_in,
        const std::array<ModuliOutT, out_limbs> &moduli_out,
        const BigInt &target_modulus,
        const BigInt &premult,
        const BigInt &postmult
    ) : premult(premult), postmult(postmult) {
        // Convert to BigInt arrays if needed
        std::array<BigInt, in_limbs> moduli_in_big = to_bigint_array(moduli_in);
        std::array<BigInt, out_limbs> moduli_out_big = to_bigint_array(moduli_out);

        std::array<std::array<uint64_t, out_limbs * OUT_DIGITS>, in_limbs * IN_DIGITS> rns_values_tf{};
        std::array<uint64_t, out_limbs * OUT_DIGITS> correction_vec{};
        std::array<BigInt, in_limbs * IN_DIGITS> sqe_tf{};

        gen_precompute_wide<in_limbs, out_limbs, IN_DIGITS, INBITS, OUT_DIGITS, OUTBITS, precision>(
            moduli_in_big, moduli_out_big, target_modulus, premult, postmult,
            rns_values_tf, correction_vec, sqe_tf, false  // Debug output disabled
        );

        // Load into members
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            rns_mat[i].load((uint64_t*)rns_values_tf[i].data());
            shifted_quotient_estimations[i] = static_cast<uint64_t>(sqe_tf[i].to_ulong());
        }
        correction.load((uint64_t*)correction_vec.data());
    }

    // Convenience constructor with default bit params (uses IN_DIGITS and OUT_DIGITS from class template, INBITS=52, OUTBITS=52)
    template<typename ModuliInT, typename ModuliOutT>
    inline RNSMatrix(
        const std::array<ModuliInT, in_limbs> &moduli_in,
        const std::array<ModuliOutT, out_limbs> &moduli_out,
        const BigInt &target_modulus,
        const BigInt &premult,
        const BigInt &postmult
    ) : premult(premult), postmult(postmult) {
        // Convert to BigInt arrays if needed
        std::array<BigInt, in_limbs> moduli_in_big = to_bigint_array(moduli_in);
        std::array<BigInt, out_limbs> moduli_out_big = to_bigint_array(moduli_out);

        // Use IN_DIGITS and OUT_DIGITS from class template, not hardcoded 1
        std::array<std::array<uint64_t, out_limbs * OUT_DIGITS>, in_limbs * IN_DIGITS> rns_values_tf{};
        std::array<uint64_t, out_limbs * OUT_DIGITS> correction_vec{};
        std::array<BigInt, in_limbs * IN_DIGITS> sqe_tf{};

        gen_precompute_wide<in_limbs, out_limbs, IN_DIGITS, 52, OUT_DIGITS, 52, precision>(
            moduli_in_big, moduli_out_big, target_modulus, premult, postmult,
            rns_values_tf, correction_vec, sqe_tf, false  // Debug output disabled
        );

        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            rns_mat[i].load((uint64_t*)rns_values_tf[i].data());
            shifted_quotient_estimations[i] = static_cast<uint64_t>(sqe_tf[i].to_ulong());
        }
        correction.load((uint64_t*)correction_vec.data());
    }

    // A version that unrolls loop by 2. Seems better for 1024 bits and worse for 2048+ bits.
    INLINE
    inline void accumulate_loop_unroll2(AVXVector<out_limbs * OUT_DIGITS> &out_hi, AVXVector<out_limbs * OUT_DIGITS> &out_lo, const AVXVector<in_limbs * IN_DIGITS> &residues, uint128_t &k_raw) const {
        // Need to allocate space with padding, although we should only iterate over the actual limbs.
        std::array<uint64_t, AVXVector<in_limbs * IN_DIGITS>::VEC_LIMBS * AVXVector<in_limbs * IN_DIGITS>::LIMBS_PER_VEC> scalars;
        residues.store(scalars.data());
        // Manually unroll loop by 2
        if constexpr (in_limbs * IN_DIGITS % 2 == 1) {
            for (int i = 0; i < in_limbs * IN_DIGITS - 2; i += 2) {
                uint64_t s0 = scalars[i];
                uint64_t s1 = scalars[i+1];
                uint64_t sqe0 = shifted_quotient_estimations[i];
                uint64_t sqe1 = shifted_quotient_estimations[i+1];
                AVXScalar sv0 = Scalar(s0);
                AVXScalar sv1 = Scalar(s1);
                auto rns_mat_i = rns_mat[i];
                auto rns_mat_i_1 = rns_mat[i+1];
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i, sv0);
                    out_lo = out_lo.mullo_scalar(rns_mat_i, sv0);
                    k_raw += (uint128_t)(s0) * (uint128_t)(sqe0);
                }
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i_1, sv1);
                    out_lo = out_lo.mullo_scalar(rns_mat_i_1, sv1);
                    k_raw += (uint128_t)(s1) * (uint128_t)(sqe1);
                }
            }
            {
                uint64_t s = scalars[in_limbs * IN_DIGITS - 1];
                AVXScalar sv = Scalar(s);
                out_hi = out_hi.mulhi_scalar(rns_mat[in_limbs * IN_DIGITS - 1], sv);
                out_lo = out_lo.mullo_scalar(rns_mat[in_limbs * IN_DIGITS - 1], sv);
                k_raw += (uint128_t)(s) * (uint128_t)(shifted_quotient_estimations[in_limbs * IN_DIGITS - 1]);
            }
        } else {
            for (int i = 0; i < in_limbs * IN_DIGITS; i += 2) {
                // auto rns_mat_i = rns_mat[i];
                // auto rns_mat_i_1 = rns_mat[i+1];
                // uint64_t s0 = scalars[i];
                // uint64_t s1 = scalars[i+1];
                // uint64_t sqe0 = shifted_quotient_estimations[i];
                // uint64_t sqe1 = shifted_quotient_estimations[i+1];
                // AVXScalar sv0 = Scalar(s0);
                // AVXScalar sv1 = Scalar(s1);
                uint64_t s0 = scalars[i];
                uint64_t s1 = scalars[i+1];
                uint64_t sqe0 = shifted_quotient_estimations[i];
                uint64_t sqe1 = shifted_quotient_estimations[i+1];
                AVXScalar sv0 = Scalar(s0);
                AVXScalar sv1 = Scalar(s1);
                auto rns_mat_i = rns_mat[i];
                auto rns_mat_i_1 = rns_mat[i+1];
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i, sv0);
                    out_lo = out_lo.mullo_scalar(rns_mat_i, sv0);
                    k_raw += (uint128_t)(s0) * (uint128_t)(sqe0);
                }
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i_1, sv1);
                    out_lo = out_lo.mullo_scalar(rns_mat_i_1, sv1);
                    k_raw += (uint128_t)(s1) * (uint128_t)(sqe1);
                }
            }
        }
    }

    INLINE
    inline void accumulate_loop(AVXVector<out_limbs * OUT_DIGITS> &out_hi, AVXVector<out_limbs * OUT_DIGITS> &out_lo, const AVXVector<in_limbs * IN_DIGITS> &residues, uint128_t &k_raw) const {
        // Need to allocate space with padding, although we should only iterate over the actual limbs.
        std::array<uint64_t, AVXVector<in_limbs * IN_DIGITS>::VEC_LIMBS * AVXVector<in_limbs * IN_DIGITS>::LIMBS_PER_VEC> scalars;
        residues.store(scalars.data());
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            uint64_t s = scalars[i];
            AVXScalar sv = Scalar(s);
            auto rns_mat_i = rns_mat[i];
            out_hi = out_hi.mulhi_scalar(rns_mat_i, sv);
            out_lo = out_lo.mullo_scalar(rns_mat_i, sv);
            k_raw += (uint128_t)(s) * (uint128_t)(shifted_quotient_estimations[i]);
        }
    }

    INLINE
    inline void accumulate_loop_interleaved(AVXVector<out_limbs * OUT_DIGITS> &out_hi, AVXVector<out_limbs * OUT_DIGITS> &out_lo, const AVXVector<in_limbs * IN_DIGITS> &residues, uint128_t &k_raw) const {
        // Need to allocate space with padding, although we should only iterate over the actual limbs.
        std::array<uint64_t, AVXVector<in_limbs * IN_DIGITS>::VEC_LIMBS * AVXVector<in_limbs * IN_DIGITS>::LIMBS_PER_VEC> scalars;
        residues.store(scalars.data());
        AVXVector<out_limbs * OUT_DIGITS> out_hi_1(0);
        AVXVector<out_limbs * OUT_DIGITS> out_lo_1(0);
        // Manually unroll loop by 2
        if constexpr (in_limbs * IN_DIGITS % 2 == 1) {
            for (int i = 0; i < in_limbs * IN_DIGITS - 2; i += 2) {
                uint64_t s0 = scalars[i];
                uint64_t s1 = scalars[i+1];
                uint64_t sqe0 = shifted_quotient_estimations[i];
                uint64_t sqe1 = shifted_quotient_estimations[i+1];
                AVXScalar sv0 = Scalar(s0);
                AVXScalar sv1 = Scalar(s1);
                auto rns_mat_i = rns_mat[i];
                auto rns_mat_i_1 = rns_mat[i+1];
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i, sv0);
                    out_lo = out_lo.mullo_scalar(rns_mat_i, sv0);
                    k_raw += (uint128_t)(s0) * (uint128_t)(sqe0);
                }
                {
                    out_hi_1 = out_hi_1.mulhi_scalar(rns_mat_i_1, sv1);
                    out_lo_1 = out_lo_1.mullo_scalar(rns_mat_i_1, sv1);
                    k_raw += (uint128_t)(s1) * (uint128_t)(sqe1);
                }
            }
            {
                uint64_t s = scalars[in_limbs * IN_DIGITS - 1];
                AVXScalar sv = Scalar(s);
                out_hi = out_hi.mulhi_scalar(rns_mat[in_limbs * IN_DIGITS - 1], sv);
                out_lo = out_lo.mullo_scalar(rns_mat[in_limbs * IN_DIGITS - 1], sv);
                k_raw += (uint128_t)(s) * (uint128_t)(shifted_quotient_estimations[in_limbs * IN_DIGITS - 1]);
            }
        } else {
            for (int i = 0; i < in_limbs * IN_DIGITS; i += 2) {
                uint64_t s0 = scalars[i];
                uint64_t s1 = scalars[i+1];
                uint64_t sqe0 = shifted_quotient_estimations[i];
                uint64_t sqe1 = shifted_quotient_estimations[i+1];
                AVXScalar sv0 = Scalar(s0);
                AVXScalar sv1 = Scalar(s1);
                auto rns_mat_i = rns_mat[i];
                auto rns_mat_i_1 = rns_mat[i+1];
                {
                    out_hi = out_hi.mulhi_scalar(rns_mat_i, sv0);
                    out_lo = out_lo.mullo_scalar(rns_mat_i, sv0);
                    k_raw += (uint128_t)(s0) * (uint128_t)(sqe0);
                }
                {
                    out_hi_1 = out_hi_1.mulhi_scalar(rns_mat_i_1, sv1);
                    out_lo_1 = out_lo_1.mullo_scalar(rns_mat_i_1, sv1);
                    k_raw += (uint128_t)(s1) * (uint128_t)(sqe1);
                }
            }
        }
        out_hi = out_hi.add(out_hi_1);
        out_lo = out_lo.add(out_lo_1);
    }

    INLINE
    inline uint64_t add_correction(AVXVector<out_limbs * OUT_DIGITS> &out_hi, AVXVector<out_limbs * OUT_DIGITS> &out_lo, uint128_t raw, const AVXVector<out_limbs * OUT_DIGITS> &c_value) const {
        uint64_t k = raw >> precision;
        /*std::cout << "  [DEBUG C++ add_correction] k_raw=" << (uint64_t)(raw >> 64) << ":" << (uint64_t)raw << ", k=" << k << std::endl;*/
        AVXScalar sk = Scalar(k);
        out_hi = out_hi.mulhi_scalar(c_value, sk);
        out_lo = out_lo.mullo_scalar(c_value, sk);
        sk = scalar_shift(sk, mulbits);
        out_hi = out_hi.mullo_scalar(c_value, sk);
        AVXVector<out_limbs * OUT_DIGITS> lp = AVXVector<out_limbs * OUT_DIGITS>(0).mulhi_scalar(c_value, sk);
        out_hi = out_hi.add(lp.slli(mulbits));
        return k;
    }

    template<int batch>
    INLINE
    static inline void add_correction_true_batch(std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_hi, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_lo, std::array<uint128_t, batch> &k_raw, const std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &c_value) {
        std::array<AVXScalar, batch> sk;
        for (int i = 0; i < batch; i++) {
            sk[i] = Scalar(k_raw[i] >> precision);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].mulhi_scalar(c_value[i], sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            out_lo[i] = out_lo[i].mullo_scalar(c_value[i], sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            sk[i] = scalar_shift(sk[i], mulbits);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].mullo_scalar(c_value[i], sk[i]);
        }
        std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> lp;
        for (int i = 0; i < batch; i++) {
            lp[i] = AVXVector<out_limbs * OUT_DIGITS>(0).mulhi_scalar(c_value[i], sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            lp[i] = lp[i].slli(mulbits);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].add(lp[i]);
        }
    }

    template<int batch>
    INLINE
    inline void add_correction_batch(std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_hi, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_lo, std::array<uint128_t, batch> &k_raw, const AVXVector<out_limbs * OUT_DIGITS> &c_value) const {
        std::array<AVXScalar, batch> sk;
        for (int i = 0; i < batch; i++) {
            sk[i] = Scalar(k_raw[i] >> precision);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].mulhi_scalar(c_value, sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            out_lo[i] = out_lo[i].mullo_scalar(c_value, sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            sk[i] = scalar_shift(sk[i], mulbits);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].mullo_scalar(c_value, sk[i]);
        }
        std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> lp;
        for (int i = 0; i < batch; i++) {
            lp[i] = AVXVector<out_limbs * OUT_DIGITS>(0).mulhi_scalar(c_value, sk[i]);
        }
        for (int i = 0; i < batch; i++) {
            lp[i] = lp[i].slli(mulbits);
        }
        for (int i = 0; i < batch; i++) {
            out_hi[i] = out_hi[i].add(lp[i]);
        }
    }

    INLINE
    // template<typename Variant>
    // __attribute__((noinline))
    inline void rns_reduce_raw(const AVXVector<in_limbs * IN_DIGITS> &residues1, AVXVector<out_limbs * OUT_DIGITS> &out_hi, AVXVector<out_limbs * OUT_DIGITS> &out_lo) const {
        uint128_t k_raw = (uint128_t)(1) << (precision - 3);
        AVXVector<out_limbs * OUT_DIGITS> out_hi_copy = out_hi; // Copying out_hi and out_lo gives significant speedup for 4096-bit moduli, for some reason.
        AVXVector<out_limbs * OUT_DIGITS> out_lo_copy = out_lo;
        accumulate_loop(out_hi_copy, out_lo_copy, residues1, k_raw);
        // accumulate_loop_interleaved(out_hi_copy, out_lo_copy, residues1, k_raw);
        // if constexpr (Variant::unroll2_) {
        //     accumulate_loop_unroll2(out_hi_copy, out_lo_copy, residues1, k_raw);
        // } else {
        //     accumulate_loop(out_hi_copy, out_lo_copy, residues1, k_raw);
        // }
        (void)add_correction(out_hi_copy, out_lo_copy, k_raw, correction);
        out_hi = out_hi_copy;
        out_lo = out_lo_copy;

        // std::array<const RNSMatrix<in_limbs, out_limbs, IN_DIGITS, OUT_DIGITS> *, 1> these = {this};
        // std::array<AVXVector<in_limbs * IN_DIGITS>, 1> residues1_copy;
        // residues1_copy[0] = residues1;
        // std::array<AVXVector<out_limbs * OUT_DIGITS>, 1> out_hi_copy;
        // out_hi_copy[0] = out_hi;
        // std::array<AVXVector<out_limbs * OUT_DIGITS>, 1> out_lo_copy;
        // out_lo_copy[0] = out_lo;
        // RNSMatrix<in_limbs, out_limbs, IN_DIGITS, OUT_DIGITS>::template rns_reduce_raw_true_batch<1>(these, residues1_copy, out_hi_copy, out_lo_copy);
        // out_hi = out_hi_copy[0];
        // out_lo = out_lo_copy[0];
    }

    template<int batch>
    INLINE
    static inline void rns_reduce_raw_true_batch(const std::array<const RNSMatrix<in_limbs, out_limbs, IN_DIGITS, OUT_DIGITS> *, batch> &these, const std::array<AVXVector<in_limbs * IN_DIGITS>, batch> &residues1, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_hi, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_lo) {
        std::array<std::array<uint64_t, AVXVector<in_limbs * IN_DIGITS>::VEC_LIMBS * AVXVector<in_limbs * IN_DIGITS>::LIMBS_PER_VEC>, batch> scalars;
        std::array<uint128_t, batch> k_raw;
        for (int i = 0; i < batch; i++) {
            residues1[i].store(scalars[i].data());
            k_raw[i] = (uint128_t)(1) << (precision - 3);
        }
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            for (int j = 0; j < batch; j++) {
                uint64_t s = scalars[j][i];
                AVXScalar sv = Scalar(s);
                out_hi[j] = out_hi[j].mulhi_scalar(these[j]->rns_mat[i], sv);
                out_lo[j] = out_lo[j].mullo_scalar(these[j]->rns_mat[i], sv);
                k_raw[j] += (uint128_t)(s) * (uint128_t)(these[j]->shifted_quotient_estimations[i]);
            }
        }
        std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> corrections;
        for (int i = 0; i < batch; i++) {
            corrections[i] = these[i]->correction;
        }
        add_correction_true_batch<batch>(out_hi, out_lo, k_raw, corrections);
    }

    template<int batch>
    INLINE
    inline void rns_reduce_raw_batch(const std::array<AVXVector<in_limbs * IN_DIGITS>, batch> &residues1, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_hi, std::array<AVXVector<out_limbs * OUT_DIGITS>, batch> &out_lo) const {
        std::array<std::array<uint64_t, AVXVector<in_limbs * IN_DIGITS>::VEC_LIMBS * AVXVector<in_limbs * IN_DIGITS>::LIMBS_PER_VEC>, batch> scalars;
        std::array<uint128_t, batch> k_raw;
        for (int i = 0; i < batch; i++) {
            residues1[i].store(scalars[i].data());
            k_raw[i] = (uint128_t)(1) << (precision - 3);
        }
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            for (int j = 0; j < batch; j++) {
                uint64_t s = scalars[j][i];
                AVXScalar sv = Scalar(s);
                out_hi[j] = out_hi[j].mulhi_scalar(rns_mat[i], sv);
                out_lo[j] = out_lo[j].mullo_scalar(rns_mat[i], sv);
                k_raw[j] += (uint128_t)(s) * (uint128_t)(shifted_quotient_estimations[i]);
            }
        }
        add_correction_batch<batch>(out_hi, out_lo, k_raw, correction);
        // for (int i = 0; i < batch; i++) {
        //     (void)add_correction(out_hi[i], out_lo[i], k_raw[i], correction);
        // }
    }

    template<class Reducer>
    INLINE
    inline AVXVector<out_limbs * OUT_DIGITS> rns_reduce_with_acc(const AVXVector<in_limbs * IN_DIGITS> &residues1, const AVXVector<out_limbs * OUT_DIGITS> &acc1, const AVXVector<out_limbs * OUT_DIGITS> &acc2, const Reducer& reducer) const {
        AVXVector<out_limbs * OUT_DIGITS> out_hi = AVXVector<out_limbs * OUT_DIGITS>(0);
        AVXVector<out_limbs * OUT_DIGITS> out_lo = AVXVector<out_limbs * OUT_DIGITS>(0);
        out_hi = out_hi.mulhi(acc1, acc2);
        out_lo = out_lo.mullo(acc1, acc2);
        rns_reduce_raw(residues1, out_hi, out_lo);
        return reducer.reduce_full(out_hi, out_lo);
    }

    template<class Reducer>
    INLINE
    inline AVXVector<out_limbs * OUT_DIGITS> rns_reduce(const AVXVector<in_limbs * IN_DIGITS> &residues1, const Reducer& reducer) const {
        /*// Debug: print scalars after word_reinterpret
        std::array<uint64_t, in_limbs * IN_DIGITS> scalars;
        residues1.store((uint64_t*)scalars.data());
        std::cout << "  [DEBUG C++ rns_reduce] scalars=[";
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            std::cout << scalars[i];
            if (i < in_limbs * IN_DIGITS - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;*/
        
        AVXVector<out_limbs * OUT_DIGITS> out_hi = AVXVector<out_limbs * OUT_DIGITS>(0);
        AVXVector<out_limbs * OUT_DIGITS> out_lo = AVXVector<out_limbs * OUT_DIGITS>(0);
        rns_reduce_raw(residues1, out_hi, out_lo);
        /*std::array<uint64_t, out_limbs * OUT_DIGITS> final_hi, final_lo;
        out_hi.store((uint64_t*)final_hi.data());
        out_lo.store((uint64_t*)final_lo.data());
        std::cout << "  [DEBUG C++ rns_reduce] before reducer.reduce_full: out_hi=[";
        for (int i = 0; i < out_limbs * OUT_DIGITS; i++) {
            std::cout << final_hi[i];
            if (i < out_limbs * OUT_DIGITS - 1) std::cout << ", ";
        }
        std::cout << "], out_lo=[";
        for (int i = 0; i < out_limbs * OUT_DIGITS; i++) {
            std::cout << final_lo[i];
            if (i < out_limbs * OUT_DIGITS - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;*/
        AVXVector<out_limbs * OUT_DIGITS> result = reducer.reduce_full(out_hi, out_lo);
        /*std::array<uint64_t, out_limbs * OUT_DIGITS> result_arr;
        result.store((uint64_t*)result_arr.data());
        std::cout << "  [DEBUG C++ rns_reduce] final result=[";
        for (int i = 0; i < out_limbs * OUT_DIGITS; i++) {
            std::cout << result_arr[i];
            if (i < out_limbs * OUT_DIGITS - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;*/
        return result;
    }

    // Test accessors
    inline void get_rns_mat(std::array<std::array<uint64_t, out_limbs * OUT_DIGITS>, in_limbs * IN_DIGITS> &out) const {
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            rns_mat[i].store((uint64_t*)out[i].data());
        }
    }
    
    inline void get_correction(std::array<uint64_t, out_limbs * OUT_DIGITS> &out) const {
        correction.store((uint64_t*)out.data());
    }
    
    inline void get_shifted_quotient_estimations(std::array<uint64_t, in_limbs * IN_DIGITS> &out) const {
        for (int i = 0; i < in_limbs * IN_DIGITS; i++) {
            out[i] = shifted_quotient_estimations[i];
        }
    }
    
    inline const BigInt& get_premult() const {
        return premult;
    }
    
    inline const BigInt& get_postmult() const {
        return postmult;
    }
};
