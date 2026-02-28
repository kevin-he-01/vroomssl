#pragma once
#include <array>
#include <stdint.h>
#include <cstring>
#include <cstdio>

// Fallback implementation of AVXVector using standard operations
// This provides the same interface as the AVX512 implementation but uses
// loops and standard operations instead of SIMD instructions

// Helper functions
inline constexpr int ceil_div(int n, int d) {
    return (n + d - 1) / d;
}

// Helper functions for 52-bit operations
constexpr uint64_t MASK_52_BITS = (1ULL << 52) - 1;

inline uint64_t apply_52bit_mask(uint64_t value) {
    return value & MASK_52_BITS;
}

// Scalar type for compatibility
typedef uint64_t AVXScalar;

inline AVXScalar Scalar(uint64_t v) {
    return v;
}

constexpr AVXScalar get_mask(int bits) {
    return ((uint64_t)(1) << bits) - 1;
}

inline AVXScalar scalar_shift(const AVXScalar &s, int bits) {
    return s >> bits;
}

// Fallback AVXVector implementation
template <int limbs>
class AVXVector {
public:
    static constexpr int LIMBS_PER_VEC = 8;  // Keep for compatibility
    static constexpr int VEC_LIMBS = (limbs + LIMBS_PER_VEC - 1) / LIMBS_PER_VEC;

private:
    std::array<uint64_t, limbs> data;

public:
    AVXVector() = default;
    
    inline AVXVector(uint64_t v) {
        for (int i = 0; i < limbs; i++) {
            data[i] = v;
        }
    }

    // Multiplication operations
    inline AVXVector mulhi(const AVXVector &a, const AVXVector &b) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            // Python: self.data[i] + (((a.data[i] & mask) * (b.data[i] & mask)) >> mulbits)
            // Mask inputs first, multiply (need 128-bit for 52-bit * 52-bit = 104-bit product), shift RIGHT, then add to data[i]
            uint64_t a_masked = a.data[i] & MASK_52_BITS;
            uint64_t b_masked = b.data[i] & MASK_52_BITS;
            __uint128_t product = (__uint128_t)a_masked * (__uint128_t)b_masked;
            uint64_t hi_part = (uint64_t)(product >> 52);  // Shift BEFORE adding
            out.data[i] = data[i] + hi_part;
        }
        return out;
    }

    inline AVXVector mullo(const AVXVector &a, const AVXVector &b) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            // Python: self.data[i] + (((a.data[i] & mask) * (b.data[i] & mask)) & mask)
            // Mask inputs first, multiply (need 128-bit for 52-bit * 52-bit = 104-bit product), mask low bits, then add to data[i]
            uint64_t a_masked = a.data[i] & MASK_52_BITS;
            uint64_t b_masked = b.data[i] & MASK_52_BITS;
            __uint128_t product = (__uint128_t)a_masked * (__uint128_t)b_masked;
            uint64_t lo_part = (uint64_t)(product & MASK_52_BITS);  // Mask low 52 bits
            out.data[i] = data[i] + lo_part;
        }
        return out;
    }

    inline AVXVector mulhi_scalar(const AVXVector &a, const AVXScalar &s) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            // Python: self.data[i] + (((a.data[i] & mask) * (s & mask)) >> mulbits)
            // Mask inputs first, multiply (need 128-bit for 52-bit * 52-bit = 104-bit product), shift RIGHT, then add to data[i]
            uint64_t a_masked = a.data[i] & MASK_52_BITS;
            uint64_t s_masked = s & MASK_52_BITS;
            __uint128_t product = (__uint128_t)a_masked * (__uint128_t)s_masked;
            uint64_t hi_part = (uint64_t)(product >> 52);  // Shift BEFORE adding
            out.data[i] = data[i] + hi_part;
        }
        return out;
    }

    inline AVXVector mullo_scalar(const AVXVector &a, const AVXScalar &s) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            // Python: self.data[i] + (((a.data[i] & mask) * (s & mask)) & mask)
            // Mask inputs first, multiply (need 128-bit for 52-bit * 52-bit = 104-bit product), mask low bits, then add to data[i]
            uint64_t a_masked = a.data[i] & MASK_52_BITS;
            uint64_t s_masked = s & MASK_52_BITS;
            __uint128_t product = (__uint128_t)a_masked * (__uint128_t)s_masked;
            uint64_t lo_part = (uint64_t)(product & MASK_52_BITS);  // Mask low 52 bits
            out.data[i] = data[i] + lo_part;
        }
        return out;
    }

    // Shift operations
    inline AVXVector srli(const int bits) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] >> bits;
        }
        return out;
    }

    inline AVXVector slli(const int bits) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] << bits;
        }
        return out;
    }

    // Arithmetic operations
    inline AVXVector add(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] + other.data[i];
        }
        return out;
    }

    inline AVXVector sub(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] - other.data[i];
        }
        return out;
    }

    // Conditional operations
    inline AVXVector cmp_sub(const AVXVector &value) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            if (data[i] >= value.data[i]) {
                out.data[i] = data[i] - value.data[i];
            } else {
                out.data[i] = data[i];
            }
        }
        return out;
    }

    inline AVXVector cmp_add(const AVXVector &v1, const AVXVector &v2, const AVXVector &val) {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            // Python: uh.mask_add(mask, modulia) where mask = h.mask_cmp_gt(hi)
            // This adds moduli[i] where h > hi, then takes mod
            // In C++, when h > hi, data[i] (which is c = hi - h) has wrapped around
            // We add moduli[i] to fix the wraparound, then take mod
            if (v1.data[i] > v2.data[i]) {
                uint64_t sum = data[i] + val.data[i];
                // Take modulo to handle potential overflow
                out.data[i] = sum % val.data[i];
            } else {
                out.data[i] = data[i] % val.data[i];
            }
        }
        return out;
    }

    // Conditional add without modulo: if v1 > v2, add val to this
    inline AVXVector mask_add_cond(const AVXVector &v1, const AVXVector &v2, const AVXVector &val) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            if (v1.data[i] > v2.data[i]) {
                out.data[i] = data[i] + val.data[i];
            } else {
                out.data[i] = data[i];
            }
        }
        return out;
    }

    // Bitwise operations
    inline AVXVector and_scalar(const AVXScalar &other) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] & other;
        }
        return out;
    }

    inline AVXVector bit_or(const AVXVector &other) const {
        AVXVector out;
        for (int i = 0; i < limbs; i++) {
            out.data[i] = data[i] | other.data[i];
        }
        return out;
    }

    inline AVXVector ctime_select(const bool selection, const AVXVector &b) const {
        // fallback, not worried about constant time, just correctness
        return selection ? *this : b;
    }

    // Access operations
    inline uint64_t extract0() const {
        return data[0];
    }

    inline void store(uint64_t* dst) const {
        for (int i = 0; i < limbs; i++) {
            dst[i] = data[i];
        }
    }

    inline void load(uint64_t* src) {
        for (int i = 0; i < limbs; i++) {
            data[i] = src[i];
        }
    }

    inline void print(const char* label) const {
        printf("%s: [", label);
        for (int i = 0; i < limbs; i++) {
            printf("%lu ", data[i]);
        }
        printf("]\n");
    }

    // Additional helper methods for debugging and compatibility
    inline uint64_t get_limb(int index) const {
        return data[index];
    }

    inline void set_limb(int index, uint64_t value) {
        data[index] = value;
    }

    // Apply 52-bit mask to all limbs
    inline void apply_mask() {
        for (int i = 0; i < limbs; i++) {
            data[i] = apply_52bit_mask(data[i]);
        }
    }
};
