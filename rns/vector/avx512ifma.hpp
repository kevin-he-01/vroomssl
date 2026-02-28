#pragma once
#include "../crypto/common.hpp"
#include <immintrin.h>
#include <stdint.h>
#include <cstdio>
#include <array>
#include <iostream>

// Class abstracting vector of length greater than 1 AVX vector.
// Perhaps if a vector only uses one limb, you can abstract accodingly


inline constexpr int ceil_div(int n, int d) {
    return (n + d - 1) / d;
}

typedef __m512i AVXScalar;

inline AVXScalar Scalar(uint64_t v) {
    return _mm512_set1_epi64(v);
}

constexpr AVXScalar get_mask(int bits) {
    uint64_t v = ((uint64_t)(1) << bits) - 1;
    int64_t vi = (int64_t)(v);
    __m512i repl = {vi,vi,vi,vi,vi,vi,vi,vi};
    return AVXScalar(repl);
}

inline AVXScalar scalar_shift(const AVXScalar &s, int bits) {
    return _mm512_srli_epi64(s, bits);
}

// number of 52 bit limbs
template <int limbs>
class AVXVector {
    // 8 52 bit limbs per AVX vector
    public:
    static constexpr int LIMBS_PER_VEC = 8;
    static constexpr int VEC_LIMBS = ceil_div(limbs, LIMBS_PER_VEC);

    private:
    alignas(64) std::array<__m512i, VEC_LIMBS> data;
    inline __m512i operator[] (const int idx) const {
        return data[idx];
    }
    inline __m512i& operator[] (const int idx) {
        return data[idx];
    }

    public:

    AVXVector() = default;
    inline AVXVector(uint64_t v) {
        for (int i = 0; i < VEC_LIMBS; i++) {
            data[i] = _mm512_set1_epi64(v);
        }
    }


    inline AVXVector mulhi(const AVXVector &a, const AVXVector &b) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_madd52hi_epu64(data[i], a[i], b[i]);
        }
        return out;
    }

    inline AVXVector mullo(const AVXVector &a, const AVXVector &b) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_madd52lo_epu64(data[i], a[i], b[i]);
        }
        return out;
    }

    inline AVXVector mulhi_scalar(const AVXVector &a, const AVXScalar &s) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_madd52hi_epu64(data[i], a[i], s);
        }
        return out;
    }

    inline AVXVector mullo_scalar(const AVXVector &a, const AVXScalar &s) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_madd52lo_epu64(data[i], a[i], s);
        }
        return out;
    }

    inline AVXVector srli(const int bits) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_srli_epi64(data[i], bits);
        }
        return out;
    }

    inline AVXVector slli(const int bits) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_slli_epi64(data[i], bits);
        }
        return out;
    }

    inline AVXVector add(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_add_epi64(data[i], other[i]);
        }
        return out;
    }

    inline AVXVector sub_min(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            __m512i diff = _mm512_sub_epi64(data[i], other[i]);
            out[i] = _mm512_min_epu64(data[i], diff);
        }
        return out;
    }

    inline AVXVector cmp_sub(const AVXVector &value) const {
        // HEXL: subtract, min
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            __mmask8 mask = _mm512_cmpge_epi64_mask(data[i], value[i]);
            out[i] = _mm512_mask_sub_epi64(data[i], mask, data[i], value[i]);
        }
        return out;
    }

    inline AVXVector cmp_add(const AVXVector &v1, const AVXVector &v2, const AVXVector &val) {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            __mmask8 mask = _mm512_cmpgt_epi64_mask(v1[i], v2[i]);
            out[i] = _mm512_mask_add_epi64(data[i], mask, data[i], val[i]);
        }
        return out;
    }

    // Conditional add without modulo: if v1 > v2, add val to this
    inline AVXVector mask_add_cond(const AVXVector &v1, const AVXVector &v2, const AVXVector &val) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            __mmask8 mask = _mm512_cmpgt_epi64_mask(v1[i], v2[i]);
            out[i] = _mm512_mask_add_epi64(data[i], mask, data[i], val[i]);
        }
        return out;
    }

    inline AVXVector and_scalar(const AVXScalar &other) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_and_epi64(data[i], other);
        }
        return out;
    }

    inline AVXVector bit_or(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_or_epi64(data[i], other[i]);
        }
        return out;
    }

    INLINE
    inline AVXVector ctime_select(const bool selection, const AVXVector &b) const {
        // If selection is true, select a
        // Blend selects b if mask is set to 1 (so if selection is false mask = 255)
        // __mmask8 = selection ? 0 : 255; // Is this constant time?
        __mmask8 mask = uint8_t(!selection) * ~uint8_t(0);
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_mask_blend_epi64(mask, data[i], b[i]);
        }
        return out;
    }

    inline AVXVector sub(const AVXVector & other) const {
        AVXVector out;
        for (int i = 0; i < VEC_LIMBS; i++) {
            out[i] = _mm512_sub_epi64(data[i], other[i]);
        }
        return out;
    }

    inline uint64_t extract0() const {
        // apparently fast?
        return *(uint64_t*)(data);
    }

    inline void store(uint64_t* dst) const {
        for (int i = 0; i < VEC_LIMBS; i++) {
            // std::cout << "LIMBS_PER_VEC*i=" << LIMBS_PER_VEC*i << std::endl;
            _mm512_storeu_epi64(dst + LIMBS_PER_VEC*i, data[i]);
        }
    }

    inline void load(uint64_t* src) {
        for (int i = 0; i < VEC_LIMBS; i++) {
            data[i] = _mm512_loadu_epi64(src + LIMBS_PER_VEC*i);
        }
    }

    inline void print(const char* label) const {
        std::array<uint64_t, VEC_LIMBS * LIMBS_PER_VEC> values;
        store(values.data());
        printf("%s: [", label);
        for (int i = 0; i < limbs; i++) {
            printf("%lu ", values[i]);
        }
        printf("]\n");
    }
};
