
#include <array>
#include <cstdint>
#include <utility>

typedef unsigned __int128 uint128_t;

// Class representing an unsigned integer of arbitrary (fixed) bit length
template<int bits>
class BigInt {
public:
    static constexpr int limbs = (bits + 63) / 64; // round up
    std::array<uint64_t, limbs> data;

    constexpr BigInt(std::array<uint64_t, limbs> d) : data(d) {}

    constexpr BigInt(uint64_t v) {
        data[0] = v;
        for (int i = 1; i < limbs; i++) {
            data[i] = 0;
        }
    }

    // Convert a base 10 string to a big integer
    // TODO: add exceptions for invalid characters and overflow
    template<std::size_t N>
    constexpr BigInt(const char(&lit)[N]) {
        BigInt value = BigInt(0);
        // N includes null terminator, so iterate from 0 to N-2
        for (std::size_t i = 0; i < N - 1; i++) {
            value = BigInt(10) * value + BigInt((uint64_t)(lit[i] - '0'));
        }
        data = value.data;
    }

    // truncate to 64 bits
    constexpr uint64_t to_ulong() const {
        return data[0];
    }

    constexpr BigInt operator+(const BigInt &other) const {
        std::array<uint64_t, limbs> result{};
        uint64_t carry = 0;
        for (std::size_t i = 0; i < limbs; i++) {
            uint128_t sum = static_cast<uint128_t>(data[i])
                                + static_cast<uint128_t>(other.data[i])
                                + carry;
            result[i] = static_cast<uint64_t>(sum);
            carry = static_cast<uint64_t>(sum >> 64);
        }
        return BigInt(result);
    }

    constexpr BigInt operator-(const BigInt &other) const {
        std::array<uint64_t, limbs> result{};
        uint64_t borrow = 0;
        for (std::size_t i = 0; i < limbs; i++) {
            uint128_t diff = static_cast<uint128_t>(data[i])
                                - static_cast<uint128_t>(other.data[i])
                                - borrow;
            result[i] = static_cast<uint64_t>(diff);
            borrow = (diff >> 127) ? 1 : 0; // Check if underflow occurred
        }
        return BigInt(result);
    }

    constexpr BigInt operator*(const BigInt &other) const {
        std::array<uint64_t, limbs> result{};
        for (std::size_t i = 0; i < limbs; i++) {
            uint64_t carry = 0;
            for (std::size_t j = 0; j < limbs; j++) {
                if (i + j < limbs) {
                    uint128_t product = static_cast<uint128_t>(data[i])
                                           * static_cast<uint128_t>(other.data[j])
                                           + static_cast<uint128_t>(result[i + j])
                                           + static_cast<uint128_t>(carry);
                    result[i + j] = static_cast<uint64_t>(product);
                    carry = static_cast<uint64_t>(product >> 64);
                } else if (carry != 0) {
                    // Overflow - can't store in result
                    break;
                }
            }
        }
        return BigInt(result);
    }

    constexpr BigInt operator/(const BigInt &other) const {
        // Binary long division algorithm
        if (other == BigInt(0)) {
            // Division by zero - in constexpr context, we can't throw
            // Return 0 as a safe default
            return BigInt(0);
        }
        
        if (*this < other) {
            return BigInt(0);
        }
        
        BigInt quotient = BigInt(0);
        BigInt remainder = *this;
        
        // Find the highest bit position of divisor
        int divisor_bits = 0;
        for (int i = limbs - 1; i >= 0; i--) {
            if (other.data[i] != 0) {
                uint64_t val = other.data[i];
                int bit_pos = 63;
                while (bit_pos >= 0 && (val >> bit_pos) == 0) {
                    bit_pos--;
                }
                divisor_bits = i * 64 + bit_pos + 1;
                break;
            }
        }
        
        // Find the highest bit position of dividend
        int dividend_bits = 0;
        for (int i = limbs - 1; i >= 0; i--) {
            if (remainder.data[i] != 0) {
                uint64_t val = remainder.data[i];
                int bit_pos = 63;
                while (bit_pos >= 0 && (val >> bit_pos) == 0) {
                    bit_pos--;
                }
                dividend_bits = i * 64 + bit_pos + 1;
                break;
            }
        }
        
        // Perform binary long division
        for (int bit_pos = dividend_bits - divisor_bits; bit_pos >= 0; bit_pos--) {
            BigInt shifted_divisor = other << static_cast<uint64_t>(bit_pos);
            if (remainder >= shifted_divisor) {
                remainder = remainder - shifted_divisor;
                int limb_idx = bit_pos / 64;
                int bit_idx = bit_pos % 64;
                if (limb_idx < limbs) {
                    quotient.data[limb_idx] |= (static_cast<uint64_t>(1) << bit_idx);
                }
            }
        }
        
        return quotient;
    }

    constexpr BigInt operator<<(const uint64_t shift_amt) const {
        if (shift_amt >= static_cast<uint64_t>(bits)) {
            return BigInt(0); // Shift beyond bit width
        }
        std::array<uint64_t, limbs> result{};
        int limb_shift = static_cast<int>(shift_amt / 64);
        int bit_shift = static_cast<int>(shift_amt % 64);
        
        for (std::size_t i = 0; i < limbs; i++) {
            std::size_t src_idx = i - static_cast<std::size_t>(limb_shift);
            
            if (src_idx < limbs && i >= static_cast<std::size_t>(limb_shift)) {
                if (bit_shift == 0) {
                    result[i] = data[src_idx];
                } else {
                    // Lower bits from this source limb
                    result[i] |= (data[src_idx] << bit_shift);
                    // Upper bits from this source limb go to next result limb
                    if (i + 1 < limbs) {
                        result[i + 1] |= (data[src_idx] >> (64 - bit_shift));
                    }
                }
            }
        }
        
        return BigInt(result);
    }

    constexpr BigInt operator>>(const uint64_t shift_amt) const {
        if (shift_amt >= static_cast<uint64_t>(bits)) {
            return BigInt(0); // Shift beyond bit width
        }
        
        std::array<uint64_t, limbs> result{};
        int limb_shift = static_cast<int>(shift_amt / 64);
        int bit_shift = static_cast<int>(shift_amt % 64);
        
        for (std::size_t i = 0; i < limbs; i++) {
            std::size_t src_idx = i + limb_shift;
            if (src_idx < limbs) {
                if (bit_shift == 0) {
                    result[i] = data[src_idx];
                } else {
                    result[i] = data[src_idx] >> bit_shift;
                    // Get upper bits from next source limb
                    if (src_idx + 1 < limbs) {
                        result[i] |= (data[src_idx + 1] << (64 - bit_shift));
                    }
                }
            }
        }
        
        return BigInt(result);
    }

    constexpr BigInt operator&(const BigInt &other) const {
        std::array<uint64_t, limbs> result{};
        for (std::size_t i = 0; i < limbs; i++) {
            result[i] = data[i] & other.data[i];
        }
        return BigInt(result);
    }

    constexpr BigInt operator%(const BigInt &other) const {
        BigInt quotient = *this / other;
        return *this - quotient * other;
    }

    // Return -1 if this < other
    // Return 0 if this == other
    // Return 1 if this > other
    constexpr int cmp(const BigInt &other) const {
        for (int i = limbs - 1; i >= 0; i--) {
            if (data[i] < other.data[i]) {
                return -1;
            } else if (data[i] > other.data[i]) {
                return 1;
            }
        }
        return 0;
    }

    constexpr bool operator<(const BigInt &other) const {
        return cmp(other) < 0;
    }

    constexpr bool operator<=(const BigInt &other) const {
        return cmp(other) <= 0;
    }
    
    constexpr bool operator>(const BigInt &other) const {
        return cmp(other) > 0;
    }

    constexpr bool operator>=(const BigInt &other) const {
        return cmp(other) >= 0;
    }

    constexpr bool operator==(const BigInt &other) const {
        return cmp(other) == 0;
    }

    // Helpful functions build from arithmetic operations
    constexpr BigInt ceil_div(const BigInt &other) const {
        return (*this + (other - BigInt(1))) / other;
    }


    constexpr BigInt gcd(const BigInt &other) const {
        BigInt a = *this;
        BigInt b = other;
        while (b != BigInt(0)) {
            BigInt temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    constexpr BigInt mod_inv(const BigInt &modulus) const {
        // Extended Euclidean algorithm for modular inverse
        // We want to find x such that (*this) * x ≡ 1 (mod modulus)
        // Algorithm based on: https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm#Modular_integers
        // To avoid negatives, check if newt will be negative because quotient*newt > t,
        // and then add modulus so it will not be negative.
        BigInt t = BigInt(0);
        BigInt newt = BigInt(1);
        BigInt r = modulus;
        BigInt newr = *this % modulus;
        
        while (newr != BigInt(0)) {
            BigInt quotient = r / newr;
            BigInt temp = t;
            t = newt;
            BigInt product = quotient * newt;
            if (product > temp) {
                BigInt diff = product - temp;
                BigInt k = (diff + modulus - BigInt(1)) / modulus;
                newt = temp + (k * modulus) - product;
            } else {
                newt = temp - product;
            }
            BigInt tempr = r;
            r = newr;
            newr = tempr - quotient * newr;
        }
        
        if (r > BigInt(1)) {
            return BigInt(0);
        }
        
        return t % modulus;
    }

    constexpr BigInt pow(const BigInt &exponent, const BigInt &modulus) const {
        BigInt result = BigInt(1);
        BigInt base = *this % modulus;
        
        // Find the highest bit set in the exponent
        int max_bit = 0;
        for (int i = limbs - 1; i >= 0; i--) {
            if (exponent.data[i] != 0) {
                uint64_t val = exponent.data[i];
                int bit_pos = 63;
                while (bit_pos >= 0 && (val >> bit_pos) == 0) {
                    bit_pos--;
                }
                max_bit = i * 64 + bit_pos;
                break;
            }
        }
        
        // Binary exponentiation
        for (int i = 0; i <= max_bit; i++) {
            int limb_idx = i / 64;
            int bit_idx = i % 64;
            if (limb_idx < limbs && (exponent.data[limb_idx] & (static_cast<uint64_t>(1) << bit_idx))) {
                result = (result * base) % modulus;
            }
            base = (base * base) % modulus;
        }
        return result;
    }

};