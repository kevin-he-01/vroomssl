#pragma once
// gpu/algorithms/baselines/CGBN/include/cgbn/cgbn.h
#include "cgbn.h"

// Precompute should be a separate kernel to make it easy to measure the performance without the precompute overhead
// Therefore, having multiple blocks (of 32 threads) compute the different values is okay.

// Simple load/store of state across kernels
// Not optimized memory accesses
template<class ValueType, int num_threads>
void store_state(ValueType* memory, const ValueType& data) {
    if (threadIdx.x < num_threads) {
        memory[threadIdx.x] = data;
    }
}

template<class ValueType, int num_threads>
ValueType load_state(ValueType* memory) {
    return memory[threadIdx.x % num_threads];
}

template<class ValueType, int num_threads>
void store_state_strided(uint32_t* memory, const ValueType& data) {
    if (threadIdx.x < num_threads) {
        constexpr int data_size = sizeof(ValueType);
        static_assert(data_size % sizeof(uint32_t) == 0, "Data size must be a multiple of uint32_t");
        uint32_t* data_ptr = reinterpret_cast<const uint32_t*>(&data);
        // Even more ideal would be to use uint128_t /256bit type, but then we may have to handle an extra few words at the end
        for (int i = 0; i < data_size / sizeof(uint32_t); i++) {
            memory[num_threads * i + threadIdx.x] = data_ptr[i];
        }
    }
}

template<class ValueType, int num_threads>
ValueType load_state_strided(const uint32_t* memory) {
    ValueType data;
    constexpr int data_size = sizeof(ValueType);
    static_assert(data_size % sizeof(uint32_t) == 0, "Data size must be a multiple of uint32_t");
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(&data);
    for (int i = 0; i < data_size / sizeof(uint32_t); i++) {
        data_ptr[i] = memory[num_threads * i + (threadIdx.x % num_threads)];
    }
    return data;
}


    template<int bits, int TPI = 32>
    class BigNum {
       
        static constexpr int rounded_bits = ceil_div(bits, TPI)*TPI;
        using env_t = cgbn_env_t<cgbn_context_t<TPI, params>, rounded_bits>;
        using bn_t = cgbn_t<env_t>;
        using wide_t = env_t::cgbn_wide_t;
        bn_t value;

        class BigNumWide {
            public:
            wide_t value;

            BigNum operator/(const BigNum& other) const {
                BigNum result;
                cgbn_div_wide(env_t, result.value, value, other.value);
                return result;
            }
    
            BigNum operator%(const BigNum& other) const {
                BigNum result;
                cgbn_mod_wide(env_t, result.value, value, other.value);
                return result;
            }

            // not constant time
            BigNum ceil_div(const BigNum& other) const {
                BigNum result;
                BigNum remainder;
                cgbn_div_rem_wide(env_t, result.value, remainder.value, value, other.value);
                if (remainder.value != 0) {
                    return result + 1;
                }
                return result;
            }
        };

        BigNum(const cgbn_mem_t<env_t::BITS>* data) {
            cgbn_load(env_t, value, data);
        }

        BigNum(const cgbn_local_t* data) {
            cgbn_load(env_t, value, data);
        }

        // Set a BigNum from a 32 bit integer
        BigNum(const uint32_t val) {
           cgbn_set_ui32(env_t, value, val);
        }

        // Get the 32 lowest bits of the big integer
        uint32_t u32() const {
            return cgbn_get_ui32(env_t, value);
        }

        // Offset in bytes, a multiple of sizeof(uint32_t)
        void store_shared_memory(int offset) const {
            extern __shared__ uint32_t shared_memory[];
            cgbn_mem_t<env_t::BITS>* dest = reinterpret_cast<cgbn_mem_t<env_t::BITS>*>(shared_memory + offset/sizeof(uint32_t));
            cgbn_store(env_t, dest, value);
        }

        void store(cgbn_mem_t<env_t::BITS>* dest) const {
            cgbn_store(env_t, dest, value);
        }

        // To have one kernel generate the numbers, and another kernel use them, assuming the same thread layout between the kernels.
        void store_local(cgbn_local_t* dest) const {
            cgbn_store(env_t, dest, value);
        }

        BigNum operator+(const BigNum& other) const {
            BigNum result;
            cgbn_add(env_t, result.value, value, other.value);
            return result;
        }

        BigNum operator-(const BigNum& other) const {
            BigNum result;
            cgbn_sub(env_t, result.value, value, other.value);
            return result;
        }

        BigNumWide operator*(const BigNum& other) const {
            BigNumWide result;
            cgbn_mul_wide(env_t, result.value, value, other.value);
            return result;
        }

        BigNum operator/(const BigNum& other) const {
            BigNum result;
            cgbn_div(env_t, result.value, value, other.value);
            return result;
        }

        BigNum operator%(const BigNum& other) const {
            BigNum result;
            cgbn_mod(env_t, result.value, value, other.value);
            return result;
        }
        
        BigNum operator<<(int shift) const {
            BigNum result;
            cgbn_shl(env_t, result.value, value, shift);
            return result;
        }

        BigNum operator>>(int shift) const {
            BigNum result;
            cgbn_shr(env_t, result.value, value, shift);
            return result;
        }

        BigNum operator&(const BigNum& other) const {
            BigNum result;
            cgbn_and(env_t, result.value, value, other.value);
            return result;
        }

        // Unary operators
        BigNum operator-() const {
            BigNum result;
            cgbn_neg(env_t, result.value, value);
            return result;
        }

        BigNum operator+() const {
            return *this;
        }

        // Comparison operators
        bool operator<(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) < 0;
        }

        bool operator>(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) > 0;
        }
        
        bool operator<=(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) <= 0;
        }

        bool operator>=(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) >= 0;
        }
        
        bool operator==(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) == 0;
        }

        bool operator!=(const BigNum& other) const {
            return cgbn_cmp(env_t, value, other.value) != 0;
        }
        
        // Bitwise operators
        BigNum operator&(const BigNum& other) const {
            BigNum result;
            cgbn_and(env_t, result.value, value, other.value);
            return result;
        }

        BigNum operator|(const BigNum& other) const {
            BigNum result;
            cgbn_or(env_t, result.value, value, other.value);
            return result;
        }

        BigNum operator^(const BigNum& other) const {
            BigNum result;
            cgbn_xor(env_t, result.value, value, other.value);
            return result;
        }
        
        // Power method with modular exponentiation
        // Invert for negative exponents
        BigNum pow(const BigNum& exponent, const BigNum& modulus) const {
            if (exponent < 0) {
                return pow(cgbn_neg(env_t, exponent), cgbn_modular_inverse(env_t, value, modulus.value));
            }
            BigNum result;
            cgbn_modular_power(env_t, result.value, value, exponent.value, modulus.value);
            return result;
        }

        BigNum mod_inverse(const BigNum& modulus) const {
            BigNum result;
            cgbn_modular_inverse(env_t, result.value, value, modulus.value);
            return result;
        }

        BigNum gcd(const BigNum& other) const {
            BigNum result;
            cgbn_gcd(env_t, result.value, value, other.value);
            return result;
        }

        BigNum mul_uint32(const uint32_t& other) const {
            BigNum result;
            cgbn_mul_ui32(env_t, result.value, value, other);
            return result;
        }

        uint32_t rem_uint32(const uint32_t& other) const {
            return cgbn_rem_ui32(env_t, value, other);
        }

        // Bit length (count leading zeros)
        uint32_t bit_length() const {
            uint32_t clz = cgbn_clz(env_t, value);
            return rounded_bits - clz;
        }

        void print_hex(thread_offset=0) const {
            for (int thread_id = thread_offset; thread_id < thread_offset + TPI; thread_id++) {
                if (threadIdx.x == thread_id) {
                    for (int i = 0; i < env_t::LIMBS; i++) {
                        if (thread_id == 0 && i == 0) {
                            printf("0x");
                        }
                        printf("%08X", value._limbs[i]);
                    }
                }
            }
        }

        BigNum from_hex(const char* hex_string) {
            BigNum result;
            cgbn_set_ui32(env_t, result.value, 0);
            constexpr int max_hex_digits = ceil_div(bits, 4);
            // Skip 0x prefix if it exists
            if (hex_string[0] == '0' && hex_string[1] == 'x') {
                hex_string += 2;
            }
            for (int i = 0; i < max_hex_digits; i++) {
                uint32_t digit = 0;
                bool reached_end = false;
                for (int j = 0; j < 4; j++) {
                    digit = digit << 4 | (hex_string[i * 4 + j] - '0');
                    // Terminate if we reach the end of the string
                    if (hex_string[i * 4 + j] == '\0') {
                        reached_end = true;
                        break;
                    }
                }
                BigNum digit_bn(digit);
                // May conflict with CGBN max SHL parameter...
                // Could use multiply instead I guess
                result = (result << 32) | digit_bn;
                // Break if we reached the end of the string
                if (reached_end) {
                    break;
                }
            }
            return result;
        }
};