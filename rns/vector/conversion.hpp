#pragma once
#include "../crypto/common.hpp"
#include "vector_impl.hpp"
#include "changebase.hpp"
#include "../reduction/montgomery.hpp"
#include "../precompute/gmp_wrapper.hpp"
#include "../fmpz_class.hh"

#include <array>
#include <cstdlib>
// #include <chrono>

// Helper function to reinterpret a BigInt as words of specified bit length (little endian)
// This is not optimized. word_reinterpret52 is far more optimized for word_length = 52
template<int limbs, int word_length>
inline void word_reinterpret(const BigInt &integer, std::array<uint64_t, limbs> &digits) {
    BigInt mask = (BigInt(1) << word_length) - 1;
    for (int i = 0; i < limbs; i++) {
        BigInt shifted = integer >> (word_length * i);
        digits[i] = static_cast<uint64_t>((shifted & mask).to_ulong());
    }
}

// Works on any little endian machine
template<int limbs>
inline void word_reinterpret52(const BigInt &integer, std::array<uint64_t, limbs> &digits) {
    constexpr size_t buf_size = (limbs - 1) / 2 * 13 + 14;
    uint8_t le_buf[buf_size];
    if (!BN_bn2le_padded(le_buf, buf_size, integer.get_value())) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "BN_bn2le_padded failed\n");
        abort();   
    }
    for (int i = 0; i < limbs; i++) {
        if (i % 2 == 0) {
            digits[i] = (*((uint64_t *) (le_buf + (i / 2) * 13))) & ((1ULL << 52) - 1);
        } else {
            digits[i] = (*((uint64_t *) (le_buf + (i / 2) * 13 + 6)) >> 4) & ((1ULL << 52) - 1);
        }
    }
}

// The convenience constructor in RNSMatrix now uses IN_DIGITS and OUT_DIGITS from class template

template<int bits, int limbs_out>
class ConvertToRNS {
    constexpr static int limbs_in = 1;  // Single modulus (target)
    constexpr static int indigits = ceil_div(bits, 52);  // Number of digits needed to represent target
    const RNSMatrix<limbs_in, limbs_out, indigits, 1> conversion_matrix;  // OUT_DIGITS=1 (not bit width!)
    public:



    inline ConvertToRNS(const BigInt &premult, const BigInt &postmult, const BigInt &target, const std::array<uint64_t, limbs_out> &moduli_out) 
        : conversion_matrix(std::array<BigInt,1>{target}, moduli_out, BigInt(0), premult, postmult) {
        // Now uses IN_DIGITS=indigits (8) and OUT_DIGITS=1 from class template
    }

    INLINE
    inline AVXVector<limbs_out> convert_to_rns(const BigInt &integer, const MontgomeryReduce<limbs_out> &reducer) const {
        std::array<uint64_t, indigits> digits;
        word_reinterpret52<indigits>(integer, digits);
        AVXVector<indigits> input;
        input.load(digits.data());
        return conversion_matrix.rns_reduce(input, reducer);
    }

    // Test accessor
    const RNSMatrix<limbs_in, limbs_out, indigits, 1>& get_conversion_matrix() const {
        return conversion_matrix;
    }
};

// Helper function to reconstruct a BigInt from words of specified bit length
// Could probably be done more efficiently with bit operations.
template<int limbs, int in_limbs, int word_length>
inline BigInt int_reconstruct(const std::array<uint64_t, in_limbs> &digits) {
    static_assert(limbs <= in_limbs, "limbs must be less than or equal to in_limbs");
    BigInt result(0);
    for (int i = 0; i < limbs; i++) {
        result += BigInt(digits[i]) << (word_length * i);
    }
    return result;
}


inline void split_52bit_limbs(uint64_t *limbs, size_t num_limbs, const FMPZ &n_in) {
    int sign = n_in.cmp(0);
    assert(sign >= 0 && "Negative numbers not supported");
    if (sign == 0) {
        memset(limbs, 0, num_limbs * sizeof(uint64_t));
        return;
    }

    nn_ptr n_limbs;
    FMPZ n(n_in);
    int num_64_limbs = fmpz_get_mpn(&n_limbs, n.get_fmpz());
    // std::cerr << "num_64_limbs: " << num_64_limbs << std::endl;
    // std::cerr << "num_limbs: " << num_limbs << std::endl;
    assert(num_limbs * 52 >= (size_t) num_64_limbs * 64 && "Not enough limbs for 52-bit representation");
    size_t n_data_size = ((num_limbs + 1) / 2) * 13 + 1; // +1 to allow efficient memcpy of size 8 bytes at a time (rather than 7, which is inefficient)
    uint8_t *n_data = new uint8_t[n_data_size];
    assert(n_data_size >= (size_t) num_64_limbs * 8);
    memset(n_data, 0, n_data_size);
    memcpy(n_data, n_limbs, num_64_limbs * 8);
    for (size_t i = 0; i < num_limbs; i++) {
        if (i % 2 == 0) {
            memcpy(limbs + i, n_data + (i / 2) * 13, 8);
        } else {
            memcpy(limbs + i, n_data + (i / 2) * 13 + 6, 8);
            limbs[i] >>= 4;
        }
        limbs[i] &= (1ULL << 52) - 1; // Mask to 52 bits
    }
    delete[] n_data;
    flint_free(n_limbs);
}

inline void fast_import_fmpz(FMPZ &n_out, uint8_t *n_data, size_t n_data_size_u64) {
    uint64_t *n_limbs = (uint64_t *)n_data;
    // Print n_limbs for debugging
    // for (size_t i = 0; i < n_data_size_u64; i++) {
    //     std::cerr << "n_limbs[" << i << "]: " << std::hex << n_limbs[i] << std::dec << std::endl;
    // }
    slong last_nonzero = n_data_size_u64 - 1;
    while (last_nonzero >= 0) {
        if (n_limbs[last_nonzero] != 0) {
            break;
        }
        --last_nonzero;
    }
    slong limb_count = last_nonzero + 1;
    // std::cerr << "limb_count: " << limb_count << std::endl;
    if (limb_count <= 1) {
        fmpz_set_ui(n_out.get_fmpz(), n_limbs[0]);
          } else {
        fmpz_set_mpn_large(n_out.get_fmpz(), n_limbs, limb_count, 0);
    }
}

inline void fast_import_bigint(BigInt &n_out, uint8_t *n_data, size_t n_data_size_u64) {
    BN_lebin2bn(n_data, n_data_size_u64 * sizeof(uint64_t), n_out.get_value());
}

// n_out = limbs[0] + (limbs[1] << 52) + (limbs[2] << 104) + ...
// Only works for max bit length = 60 (can do left shift by 4 and still not overflow uint64_t)
template<size_t num_limbs>
inline void reconstruct_from_ovf_52bit_limbs(BigInt &n_out, const std::array<uint64_t, num_limbs> &limbs) {
    // Upper 12 bits of limbs[i] need not be zero. This will be incorporated correctly.
    constexpr size_t max_i_1 = (num_limbs - 1) / 2 * 2;
    constexpr size_t min_data_size_loop1 = max_i_1 / 2 * 13 + sizeof(uint64_t);
    constexpr size_t max_i_2 = num_limbs - 1 - num_limbs % 2;
    constexpr size_t min_data_size_loop2 = max_i_2 / 2 * 13 + 6 + sizeof(uint64_t);
    constexpr size_t n_data_size_needed = std::max(min_data_size_loop1, min_data_size_loop2);
    constexpr size_t n_data_size_u64 = (n_data_size_needed + 7) / 8; // Number of 64-bit limbs
    constexpr size_t n_data_size = n_data_size_u64 * 8;
    static_assert(sizeof(BN_ULONG) == sizeof(uint64_t), "BN_ULONG must be the same size as uint64_t (64-bit platform)");
    BN_ULONG n_data_even_u64[n_data_size_u64];
    BN_ULONG n_data_odd_u64[n_data_size_u64];
    // uint8_t n_data_even[n_data_size]; // The 1st, 3rd, 5th... limbs (even in 0-based indexing)
    // uint8_t n_data_odd[n_data_size]; // The 2nd, 4th, 6th... limbs
    uint8_t *n_data_even = (uint8_t *) n_data_even_u64;
    uint8_t *n_data_odd = (uint8_t *) n_data_odd_u64;
    memset(n_data_even, 0, n_data_size);
    memset(n_data_odd, 0, n_data_size);
    static_assert(min_data_size_loop1 <= n_data_size, "n_data_size is too small"); // compile-time bound check
    for (size_t i = 0; i < num_limbs; i += 2) {
        // memcpy(n_data_even + (i / 2) * 13, limbs + i, 8);
        *(uint64_t *)(n_data_even + (i / 2) * 13) = limbs[i];
    }
    static_assert(min_data_size_loop2 <= n_data_size, "n_data_size is too small"); // compile-time bound check
    for (size_t i = 1; i < num_limbs; i += 2) {
        *(uint64_t *)(n_data_odd + (i / 2) * 13 + 6) = limbs[i] << 4;
    }
    fast_import_bigint(n_out, n_data_even, n_data_size_u64);
    BigInt tmp;
    fast_import_bigint(tmp, n_data_odd, n_data_size_u64);
    bn_uadd_consttime(n_out.get_value(), n_out.get_value(), tmp.get_value());
}

template<size_t num_limbs, size_t n_out_width_u64, size_t n_out_width_alloc_u64 = n_out_width_u64>
inline void reconstruct_from_ovf_52bit_limbs_fast(BigInt &n_out, const std::array<uint64_t, num_limbs> &limbs) {
    static_assert(n_out_width_alloc_u64 >= n_out_width_u64, "n_out_width_alloc_u64 must be greater than or equal to n_out_width_u64 for this implementation to be correct");
    // Upper 12 bits of limbs[i] need not be zero. This will be incorporated correctly.
    constexpr size_t max_i_1 = (num_limbs - 1) / 2 * 2;
    constexpr size_t min_data_size_loop1 = max_i_1 / 2 * 13 + sizeof(uint64_t);
    constexpr size_t max_i_2 = num_limbs - 1 - num_limbs % 2;
    constexpr size_t min_data_size_loop2 = max_i_2 / 2 * 13 + 6 + sizeof(uint64_t);
    constexpr size_t n_data_size_needed = std::max(min_data_size_loop1, min_data_size_loop2);
    constexpr size_t n_data_size_u64 = std::max(n_out_width_u64, (n_data_size_needed + 7) / 8); // Number of 64-bit limbs
    constexpr size_t n_data_size = n_data_size_u64 * 8;
    static_assert(sizeof(BN_ULONG) == sizeof(uint64_t), "BN_ULONG must be the same size as uint64_t (64-bit platform)");
    BN_ULONG n_data_even_u64[n_data_size_u64];
    BN_ULONG n_data_odd_u64[n_data_size_u64];
    // uint8_t n_data_even[n_data_size]; // The 1st, 3rd, 5th... limbs (even in 0-based indexing)
    // uint8_t n_data_odd[n_data_size]; // The 2nd, 4th, 6th... limbs
    uint8_t *n_data_even = (uint8_t *) n_data_even_u64;
    uint8_t *n_data_odd = (uint8_t *) n_data_odd_u64;
    memset(n_data_even, 0, n_data_size);
    memset(n_data_odd, 0, n_data_size);
    static_assert(min_data_size_loop1 <= n_data_size, "n_data_size is too small"); // compile-time bound check
    for (size_t i = 0; i < num_limbs; i += 2) {
        // memcpy(n_data_even + (i / 2) * 13, limbs + i, 8);
        *(uint64_t *)(n_data_even + (i / 2) * 13) = limbs[i];
    }
    static_assert(min_data_size_loop2 <= n_data_size, "n_data_size is too small"); // compile-time bound check
    for (size_t i = 1; i < num_limbs; i += 2) {
        *(uint64_t *)(n_data_odd + (i / 2) * 13 + 6) = limbs[i] << 4;
    }
    // fast_import_bigint(n_out, n_data_even, n_data_size_u64);
    // BigInt tmp;
    // fast_import_bigint(tmp, n_data_odd, n_data_size_u64);
    // bn_uadd_consttime(n_out.get_value(), n_out.get_value(), tmp.get_value());
    if (!bn_wexpand(n_out.get_value(), n_out_width_alloc_u64)) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    static_assert(n_data_size_u64 >= n_out_width_u64, "n_data_size_u64 must be greater than or equal to n_out_width_u64 for this implementation to be correct");
    bn_add_words(n_out.get_value()->d, n_data_even_u64, n_data_odd_u64, n_out_width_u64);
    n_out.get_value()->width = n_out_width_u64;
}

// inline void reconstruct_from_ovf_52bit_limbs_fmpz(FMPZ &n_out, const uint64_t *limbs, size_t num_limbs) {
//     // Upper 12 bits of limbs[i] need not be zero. This will be incorporated correctly.
//     size_t n_data_size_u64 = ((num_limbs + 1) / 2) * 13 / 8 + 1; // Number of 64-bit limbs
//     size_t n_data_size = n_data_size_u64 * 8;
//     uint8_t *n_data_even = new uint8_t[n_data_size]; // The 1st, 3rd, 5th... limbs (even in 0-based indexing)
//     uint8_t *n_data_odd = new uint8_t[n_data_size]; // The 2nd, 4th, 6th... limbs
//     memset(n_data_even, 0, n_data_size);
//     memset(n_data_odd, 0, n_data_size);
//     for (size_t i = 0; i < num_limbs; i += 2) {
//         memcpy(n_data_even + (i / 2) * 13, limbs + i, 8);
//     }
//     for (size_t i = 1; i < num_limbs; i += 2) {
//         memcpy(n_data_odd + (i / 2) * 13, limbs + i, 8);
//     }
//     fast_import_fmpz(n_out, n_data_even, n_data_size_u64);
//     FMPZ tmp;
//     fast_import_fmpz(tmp, n_data_odd, n_data_size_u64);
//     tmp.mul_2exp(tmp, 52);
//     n_out.add(n_out, tmp);
//     delete[] n_data_odd;
//     delete[] n_data_even;
// }

// #define DEBUG_MAX_BIT_LENGTH

template<int bits,int limbs_in>
class ConvertFromRNS {
    // Python uses word_len(2*target, outbase) = (bit_length(2*target) + outbase - 1) // outbase
    // For 381-bit target, if applied to the output of a multiply, could be up to 3*target, so we need to add extra bits to handle the redundant output in binary.
    // The conversion process introduces more redundance, so a total of 4 bits are needed.
    static constexpr int target_width_u64 = ceil_div(bits, 64);
    static constexpr int outdigits = ceil_div(bits + 4, 52);
    static constexpr int outdigits_rounded = AVXVector<outdigits>::VEC_LIMBS * AVXVector<outdigits>::LIMBS_PER_VEC; // Rounds up to the nearest LIMBS_PER_VEC. Needed to prevent buffer overflow in store
    const RNSMatrix<limbs_in, 1, 1, outdigits> conversion_matrix;  // OUT_DIGITS=outdigits
    const BigInt target;
    const uint64_t target_inverse;
    public:
    inline ConvertFromRNS(const BigInt &premult, const BigInt &postmult, const BigInt &target, const std::array<uint64_t, limbs_in> &moduli_in)
        : conversion_matrix(moduli_in, std::array<BigInt,1>{target}, BigInt(0), premult, postmult), target(target), target_inverse((-target).mod_inverse(BigInt(1) << 64).to_ulong()) {}

    INLINE
    inline BigInt convert_from_rns(const AVXVector<limbs_in> &residues) const {
        AVXVector<outdigits> out_hi = AVXVector<outdigits>(0);
        AVXVector<outdigits> out_lo = AVXVector<outdigits>(0);
        // auto start = std::chrono::high_resolution_clock::now();
        conversion_matrix.rns_reduce_raw(residues, out_hi, out_lo);
        // auto reduce_raw = std::chrono::high_resolution_clock::now();
        // Combine hi and lo into a single BigInt
        std::array<uint64_t, outdigits_rounded> digits_lo;
        std::array<uint64_t, outdigits_rounded> digits_hi;
        static_assert(outdigits <= outdigits_rounded, "outdigits must be less than outdigits_rounded");
        // std::cerr << "outdigits=" << outdigits << std::endl;
        // std::cerr << "outdigits_rounded=" << outdigits_rounded << std::endl;
        out_hi.store(digits_hi.data());
        out_lo.store(digits_lo.data());
        // Correctness of this code depends on max_bit_length <= 59, 2^(59-52) = 128
        static_assert(limbs_in <= 128, "limbs_in must be less than or equal to 128 for this implementation to be correct");
        // Print bit lengths of each element of digits_lo and digits_hi
#ifdef DEBUG_MAX_BIT_LENGTH
        size_t max_bit_length = 0;
        for (size_t i = 0; i < outdigits_rounded; i++) {
            // std::cerr << "digits_lo[" << i << "] = " << std::hex << digits_lo[i] << std::dec << ", bits = " << BigInt(digits_lo[i]).bit_length() << std::endl;
            if (BigInt(digits_lo[i]).bit_length() > max_bit_length) {
                max_bit_length = BigInt(digits_lo[i]).bit_length();
            }
        }
        for (size_t i = 0; i < outdigits_rounded; i++) {
            // std::cerr << "digits_hi[" << i << "] = " << std::hex << digits_hi[i] << std::dec << ", bits = " << BigInt(digits_hi[i]).bit_length() << std::endl;
            if (BigInt(digits_hi[i]).bit_length() > max_bit_length) {
                max_bit_length = BigInt(digits_hi[i]).bit_length();
            }
        }
        std::cerr << "bits = " << bits << ", limbs_in = " << limbs_in << ", max_bit_length = " << max_bit_length << std::endl;
#endif
        // auto store = std::chrono::high_resolution_clock::now();
        // Eventually we could rotate vectors and use avx512 addition (creating 53 bit numbers), and then do the extraction.
        // We could also get the quotient directly from the matrix instead of % target, to within a factor of 2 (therefore requiring only a conditional correction).

        // bn_uadd_consttime(output.get_value(), output.get_value(), tmp.get_value());

        std::array<uint64_t, outdigits + 1> digits; // Could have also used vperm to do a cyclic shift on out_hi, do AVX512::add, and store that into digits to obtain this (be sure to use outdigits_rounded in this case).
        // Max bit length <= 1+59 = 60 due to the sum of two 59-bit numbers.
        digits[0] = digits_lo[0];
        for (int i = 1; i < outdigits; i++) {
            digits[i] = digits_hi[i - 1] + digits_lo[i];
        }
        digits[outdigits] = digits_hi[outdigits - 1];
        // for (int i = 0; i < outdigits + 1; i++) {
        //     std::cerr << "digits[" << i << "] = " << std::hex << digits[i] << std::dec << std::endl;
        // }
        BigInt output;
        // Only need to work for max bit length = 60 (can do left shift by 4 and still not overflow uint64_t)
        reconstruct_from_ovf_52bit_limbs_fast<static_cast<size_t>(outdigits + 1), target_width_u64 + 1>(output, digits);
        // std::cerr << "output = " << std::hex << output.get_hex_str() << std::dec << std::endl;

        // assert(output.get_value()->width >= target_width_u64 + 1); // should be true if output is computed using constant-time code (such that width may not be minimal even when there are leading zeros)
        // output.get_value()->width = target_width_u64 + 1; // uadd_consttime is pessimistic, actual bound is at most one more than the width of the target modulus p
        
        // BigInt quotient = output / target;
        // if (BN_num_bits(quotient.get_value()) >= 64) {
            // std::cerr << "quotient: " << std::hex << quotient << std::dec << std::endl;
            // std::cerr << "target bits = " << BN_num_bits(target.get_value()) << std::endl;
            // std::cerr << "limbs_in * 52 = " << limbs_in * 52 << std::endl;
            // std::cerr << "output bits = " << BN_num_bits(output.get_value()) << std::endl;
            // std::cerr << "quotient bits = " << BN_num_bits(quotient.get_value()) << std::endl;
            // abort();
        // }
        // output %= target; // Non constant-time code
        // Constant-time version of the above:
        // In RSA, BN_num_bits(target.get_value()) is public
        // bn_div_consttime(nullptr, output.get_value(), output.get_value(), target.get_value(), BN_num_bits(target.get_value()), target.get_ctx());

        // auto reconstruct = std::chrono::high_resolution_clock::now();
        // abort();

        // Montgomery reduce by one limb
        // uint64_t c0 = output.to_ulong();
        // BigInt mu = BigInt(c0 * (-target).mod_inverse(BigInt(1) << 64).to_ulong());
        // // std::cerr << "Expect 0: " << (output + mu * target).to_ulong() << std::endl;
        // BigInt output_redundant = (output + mu * target) >> 64;
        // if (output_redundant >= target) {
        //     // std::cerr << "output_redundant >= target!" << std::endl;
        //     output_redundant -= target;
        // }
        // return output_redundant;

        uint64_t c0 = output.get_value()->d[0];
        uint64_t mu = c0 * target_inverse;
        
        assert(target.get_value()->width >= target_width_u64);

        // BigInt tmp;
        // // std::cerr << "tmp internal width (before mul): " << tmp.get_value()->width << std::endl;
        // if (!bn_wexpand(tmp.get_value(), target_width_u64 + 1)) {
        //     ERR_print_errors_fp(stderr);
        //     abort();
        // }
        // uint64_t carry = bn_mul_words(tmp.get_value()->d, target.get_value()->d, target_width_u64, mu);
        // tmp.get_value()->d[target_width_u64] = carry;
        // tmp.get_value()->width = target_width_u64 + 1;
        // bn_uadd_consttime(output.get_value(), output.get_value(), tmp.get_value());

        

        BN_ULONG carry = bn_mul_add_words(output.get_value()->d, target.get_value()->d, target_width_u64, mu);
        output.get_value()->d[target_width_u64] = CRYPTO_addc_w(output.get_value()->d[target_width_u64], 0, carry, &carry);
        // The following lines are needed if you need to operate on `output` using any high-level API (even just print the number itself)
        // if (!bn_wexpand(output.get_value(), target_width_u64 + 2)) {
        //     ERR_print_errors_fp(stderr);
        //     abort();
        // }
        // output.get_value()->d[target_width_u64 + 1] = carry;
        // output.get_value()->width = target_width_u64 + 2;

        // Old code involves a copy
        // OPENSSL_memmove(output.get_value()->d, output.get_value()->d + 1, (output.get_value()->width - 1) * sizeof(uint64_t));
        // output.get_value()->width--;
        // assert(output.get_value()->width >= target_width_u64 + 1);
        // output.get_value()->width = target_width_u64 + 1;

        BigInt return_value;
        if (!bn_wexpand(return_value.get_value(), target_width_u64)) {
            ERR_print_errors_fp(stderr);
            abort();
        }
        return_value.get_value()->width = target_width_u64;
        // Old code involves a copy
        // bn_reduce_once(return_value.get_value()->d, output.get_value()->d, output.get_value()->d[target_width_u64], target.get_value()->d, target_width_u64);
        bn_reduce_once(return_value.get_value()->d, output.get_value()->d + 1, carry, target.get_value()->d, target_width_u64);
        return return_value;
    }
    
    // Test accessor
    const RNSMatrix<limbs_in, 1, 1, outdigits>& get_conversion_matrix() const {
        return conversion_matrix;
    }
};

// template<int bits,int limbs_in>
// class ConvertFromRNS {
//     // Python uses word_len(2*target, outbase) = (bit_length(2*target) + outbase - 1) // outbase
//     // For 381-bit target, if applied to the output of a multiply, could be up to 3*target, so we need to add extra bits to handle the redundant output in binary.
//     // The conversion process introduces more redundance, so a total of 4 bits are needed.
//     static constexpr int outdigits = ceil_div(bits + 4, 52);
//     static constexpr int outdigits_rounded = AVXVector<outdigits>::VEC_LIMBS * AVXVector<outdigits>::LIMBS_PER_VEC; // Rounds up to the nearest LIMBS_PER_VEC. Needed to prevent buffer overflow in store
//     const RNSMatrix<limbs_in, 1, 1, outdigits> conversion_matrix;  // OUT_DIGITS=outdigits
//     BigInt target;
//     public:
//     inline ConvertFromRNS(const BigInt &premult, const BigInt &postmult, const BigInt &target, const std::array<uint64_t, limbs_in> &moduli_in)
//         : conversion_matrix(moduli_in, std::array<BigInt,1>{target}, BigInt(0), premult, postmult), target(target) {}

//     inline BigInt convert_from_rns(const AVXVector<limbs_in> &residues) const {
//         AVXVector<outdigits> out_hi = AVXVector<outdigits>(0);
//         AVXVector<outdigits> out_lo = AVXVector<outdigits>(0);
//         // auto start = std::chrono::high_resolution_clock::now();
//         conversion_matrix.rns_reduce_raw(residues, out_hi, out_lo);
//         // auto reduce_raw = std::chrono::high_resolution_clock::now();
//         // Combine hi and lo into a single BigInt
//         std::array<uint64_t, outdigits_rounded> digits_lo;
//         std::array<uint64_t, outdigits_rounded> digits_hi;
//         // std::cerr << "outdigits=" << outdigits << std::endl;
//         // std::cerr << "outdigits_rounded=" << outdigits_rounded << std::endl;
//         out_lo.store(digits_lo.data());
//         out_hi.store(digits_hi.data());
//         // auto store = std::chrono::high_resolution_clock::now();
//         // Eventually we could rotate vectors and use avx512 addition (creating 53 bit numbers), and then do the extraction.
//         // We could also get the quotient directly from the matrix instead of % target, to within a factor of 2 (therefore requiring only a conditional correction).
//         BigInt i = int_reconstruct<outdigits, outdigits_rounded, 52>(digits_lo);
//         i += int_reconstruct<outdigits, outdigits_rounded, 52>(digits_hi) << 52;
//         // auto reconstruct = std::chrono::high_resolution_clock::now();
//         auto result = i % target;
//         // auto end = std::chrono::high_resolution_clock::now();
//         // std::cerr << "rns_reduce_raw time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(reduce_raw - start).count() << " ns" << std::endl;
//         // std::cerr << "store time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(store - reduce_raw).count() << " ns" << std::endl;
//         // std::cerr << "reconstruct time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(reconstruct - store).count() << " ns" << std::endl;
//         // std::cerr << "mod time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - reconstruct).count() << " ns" << std::endl;
//         // std::cerr << "total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;
//         return result;
//     }
    
//     // Test accessor
//     const RNSMatrix<limbs_in, 1, 1, outdigits>& get_conversion_matrix() const {
//         return conversion_matrix;
//     }
// };

// New version below using FMPZ
// template<int bits,int limbs_in>
// class ConvertFromRNS {
//     // Python uses word_len(2*target, outbase) = (bit_length(2*target) + outbase - 1) // outbase
//     // For 381-bit target, if applied to the output of a multiply, could be up to 3*target, so we need to add extra bits to handle the redundant output in binary.
//     // The conversion process introduces more redundance, so a total of 4 bits are needed.
//     static constexpr int outdigits = ceil_div(bits + 4, 52);
//     static constexpr int outdigits_rounded = AVXVector<outdigits>::VEC_LIMBS * AVXVector<outdigits>::LIMBS_PER_VEC; // Rounds up to the nearest LIMBS_PER_VEC. Needed to prevent buffer overflow in store
//     const RNSMatrix<limbs_in, 1, 1, outdigits> conversion_matrix;  // OUT_DIGITS=outdigits
//     const FMPZ target_fmpz;
//     public:
//     inline ConvertFromRNS(const BigInt &premult, const BigInt &postmult, const BigInt &target, const std::array<uint64_t, limbs_in> &moduli_in)
//         : conversion_matrix(moduli_in, std::array<BigInt,1>{target}, BigInt(0), premult, postmult), target_fmpz(target.get_mpz()) {}

//     inline BigInt convert_from_rns(const AVXVector<limbs_in> &residues) const {
//         AVXVector<outdigits> out_hi = AVXVector<outdigits>(0);
//         AVXVector<outdigits> out_lo = AVXVector<outdigits>(0);
//         // auto start = std::chrono::high_resolution_clock::now();
//         conversion_matrix.rns_reduce_raw(residues, out_hi, out_lo);
//         // auto reduce_raw = std::chrono::high_resolution_clock::now();
//         // Combine hi and lo into a single BigInt
//         std::array<uint64_t, outdigits_rounded> digits_lo;
//         std::array<uint64_t, outdigits_rounded + 1> digits_hi;
//         digits_hi[0] = 0;
//         // std::cerr << "outdigits=" << outdigits << std::endl;
//         // std::cerr << "outdigits_rounded=" << outdigits_rounded << std::endl;
//         out_hi.store(digits_hi.data() + 1);
//         out_lo.store(digits_lo.data());
//         // auto store = std::chrono::high_resolution_clock::now();
//         // Eventually we could rotate vectors and use avx512 addition (creating 53 bit numbers), and then do the extraction.
//         // We could also get the quotient directly from the matrix instead of % target, to within a factor of 2 (therefore requiring only a conditional correction).
//         // BigInt i = int_reconstruct<outdigits, outdigits_rounded, 52>(digits_lo);
//         // i += int_reconstruct<outdigits, outdigits_rounded, 52>(digits_hi) << 52;
//         FMPZ output;
//         reconstruct_from_ovf_52bit_limbs(output, digits_lo.data(), outdigits);
//         FMPZ tmp;
//         reconstruct_from_ovf_52bit_limbs(tmp, digits_hi.data(), outdigits + 1);
//         output.add(output, tmp);
//         output.mod(output, target_fmpz);
//         // auto reconstruct = std::chrono::high_resolution_clock::now();
//         return BigInt(output);
//         // auto end = std::chrono::high_resolution_clock::now();
//         // std::cerr << "rns_reduce_raw time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(reduce_raw - start).count() << " ns" << std::endl;
//         // std::cerr << "store time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(store - reduce_raw).count() << " ns" << std::endl;
//         // std::cerr << "reconstruct time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(reconstruct - store).count() << " ns" << std::endl;
//         // std::cerr << "mod time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - reconstruct).count() << " ns" << std::endl;
//         // std::cerr << "total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;
//         // return result;
//     }
    
//     // Test accessor
//     const RNSMatrix<limbs_in, 1, 1, outdigits>& get_conversion_matrix() const {
//         return conversion_matrix;
//     }
// };
