#pragma once
#include "../vector/vector_impl.hpp"
#include "../precompute/gmp_wrapper.hpp"
#ifndef USE_AVX512_FALLBACK
#include <immintrin.h>
#endif

template <int limbs>
class MontgomeryReduce {

    AVXVector<limbs> mont_factor;
    AVXVector<limbs> moduli;
    AVXVector<limbs> t_squared;
    
    public:
    // Expose moduli for use in cmp_sub operations
    const AVXVector<limbs>& get_moduli() const { return moduli; }

    static constexpr int mulbits = 52;
    static constexpr int bits = 52;
    static constexpr int hi_bit_shift = bits + bits - mulbits;

    const AVXScalar hi_mask = get_mask(hi_bit_shift);
    const AVXScalar lo_mask = get_mask(mulbits);

    // Constructor that computes parameters from moduli, matching python_reduction.MontgomeryReduce
    inline MontgomeryReduce(const std::array<uint64_t, limbs> &moduli_in) {
#ifdef USE_AVX512_FALLBACK
        constexpr int LOAD_SIZE = limbs;
#else
        constexpr int LOAD_SIZE = AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC;
#endif
        uint64_t moduli_arr[LOAD_SIZE];
        uint64_t mont_arr[LOAD_SIZE];
        uint64_t tsq_arr[LOAD_SIZE];

        // Prepare BigInt constants
        BigInt r_mod = BigInt(1) << mulbits;           // 2^mulbits
        BigInt t2 = BigInt(1) << (2 * bits);           // 2^(2*bits)

        for (int i = 0; i < limbs; i++) {
            moduli_arr[i] = moduli_in[i];
            BigInt m(moduli_in[i]);
            BigInt inv = m.mod_inverse(r_mod);
            mont_arr[i] = static_cast<uint64_t>(inv.to_ulong());
            BigInt tsq = t2 % m;
            tsq_arr[i] = static_cast<uint64_t>(tsq.to_ulong());
        }
        // If vector width exceeds limbs, zero the rest
        for (int i = limbs; i < LOAD_SIZE; i++) {
            moduli_arr[i] = 0;
            mont_arr[i] = 0;
            tsq_arr[i] = 0;
        }

        this->moduli.load(moduli_arr);
        this->mont_factor.load(mont_arr);
        this->t_squared.load(tsq_arr);
    }

    inline AVXVector<limbs> reduce_small(const AVXVector<limbs> &hi, const AVXVector<limbs> &low) const {
        auto q = AVXVector<limbs>(0).mullo(low, mont_factor);
        auto h = AVXVector<limbs>(0).mulhi(q, moduli);
        auto c = hi.sub(h);
        auto result = c.cmp_add(h, hi, moduli);
        return result;
    }

    template <int batch>
    inline std::array<AVXVector<limbs>, batch> reduce_small_batch(const std::array<AVXVector<limbs>, batch> &hi, const std::array<AVXVector<limbs>, batch> &low) const {
        std::array<AVXVector<limbs>, batch> q;
        for (int i = 0; i < batch; i++) {
            q[i] = AVXVector<limbs>(0).mullo(low[i], mont_factor);
        }
        std::array<AVXVector<limbs>, batch> h;
        for (int i = 0; i < batch; i++) {
            h[i] = AVXVector<limbs>(0).mulhi(q[i], moduli);
        }
        std::array<AVXVector<limbs>, batch> c;
        for (int i = 0; i < batch; i++) {
            c[i] = hi[i].sub(h[i]);
        }
        std::array<AVXVector<limbs>, batch> result;
        for (int i = 0; i < batch; i++) {
            result[i] = c[i].cmp_add(h[i], hi[i], moduli);
        }
        return result;
    }

    template <int batch>
    static inline std::array<AVXVector<limbs>, batch> reduce_small_batch_diff_moduli(const std::array<MontgomeryReduce<limbs>, batch> &these, const std::array<AVXVector<limbs>, batch> &hi, const std::array<AVXVector<limbs>, batch> &low) {
        std::array<AVXVector<limbs>, batch> q;
        for (int i = 0; i < batch; i++) {
            q[i] = AVXVector<limbs>(0).mullo(low[i], these[i].mont_factor);
        }
        std::array<AVXVector<limbs>, batch> h;
        for (int i = 0; i < batch; i++) {
            h[i] = AVXVector<limbs>(0).mulhi(q[i], these[i].moduli);
        }
        std::array<AVXVector<limbs>, batch> c;
        for (int i = 0; i < batch; i++) {
            c[i] = hi[i].sub(h[i]);
        }
        std::array<AVXVector<limbs>, batch> result;
        for (int i = 0; i < batch; i++) {
            result[i] = c[i].cmp_add(h[i], hi[i], these[i].moduli);
        }
        return result;
    }

    inline AVXVector<limbs> reduce_full(const AVXVector<limbs> &hi, const AVXVector<limbs> &low) const {
        auto hi_hi = hi.srli(hi_bit_shift);
        auto hi_rem = hi.and_scalar(hi_mask);
        auto lo_acc = low.mullo(hi_hi, t_squared);

        auto lo_hi = lo_acc.srli(mulbits);
        auto lo_rem = lo_acc.and_scalar(lo_mask);
        auto hi_acc = hi_rem.add(lo_hi);
        auto hi_cor = hi_acc.cmp_sub(moduli);
        auto result = reduce_small(hi_cor, lo_rem);
        return result;
    }
    
    template <int batch>
    inline std::array<AVXVector<limbs>, batch> reduce_full_batch(const std::array<AVXVector<limbs>, batch> &hi, const std::array<AVXVector<limbs>, batch> &low) const {
        std::array<AVXVector<limbs>, batch> hi_hi;
        for (int i = 0; i < batch; i++) {
            hi_hi[i] = hi[i].srli(hi_bit_shift);
        }
        std::array<AVXVector<limbs>, batch> hi_rem;
        for (int i = 0; i < batch; i++) {
            hi_rem[i] = hi[i].and_scalar(hi_mask);
        }
        std::array<AVXVector<limbs>, batch> lo_acc;
        for (int i = 0; i < batch; i++) {
            lo_acc[i] = low[i].mullo(hi_hi[i], t_squared);
        }
        std::array<AVXVector<limbs>, batch> lo_hi;
        for (int i = 0; i < batch; i++) {
            lo_hi[i] = lo_acc[i].srli(mulbits);
        }

        std::array<AVXVector<limbs>, batch> lo_rem;
        for (int i = 0; i < batch; i++) {
            lo_rem[i] = lo_acc[i].and_scalar(lo_mask);
        }
        std::array<AVXVector<limbs>, batch> hi_acc;
        for (int i = 0; i < batch; i++) {
            hi_acc[i] = hi_rem[i].add(lo_hi[i]);
        }
        std::array<AVXVector<limbs>, batch> hi_cor;
        for (int i = 0; i < batch; i++) {
            hi_cor[i] = hi_acc[i].cmp_sub(moduli);
        }
        return reduce_small_batch<batch>(hi_cor, lo_rem);
    }

    // inline AVXVector<limbs> psuedo_reduce_50(const AVXVector<limbs> &v) const {
    //     auto hi = v.srli(bits);
    //     auto lo = v.and_scalar(lo_mask);
    //     return lo.mullo(hi, t);
    // }

    // Test accessors
    inline void get_moduli(std::array<uint64_t, limbs> &out) const {
        moduli.store((uint64_t*)out.data());
    }
    
    inline void get_mont_factor(std::array<uint64_t, limbs> &out) const {
        mont_factor.store((uint64_t*)out.data());
    }
    
    inline void get_t_squared(std::array<uint64_t, limbs> &out) const {
        t_squared.store((uint64_t*)out.data());
    }
    
    static constexpr int get_mulbits() { return mulbits; }
    static constexpr int get_bits() { return bits; }
    static constexpr int get_hi_bit_shift() { return hi_bit_shift; }

};
