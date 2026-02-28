#ifndef GMP_WRAPPER_HPP
#define GMP_WRAPPER_HPP

#include <gmpxx.h>
#include <openssl/bn.h>
#include <string>
#include <iostream>
#include <ctime>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include "../../fmpz_class.hh"

namespace gmp_wrapper {
class BigInt {
private:
    mpz_class value;

public:
    // Helper to get decimal string representation for logging
    std::string get_decimal_str() const {
        return value.get_str(10);
    }
    std::string get_hex_str() const {
        return value.get_str(16);
    }
    // Constructors
    BigInt() : value(0) {
        // std::cerr << "BigInt::BigInt() -> " << get_decimal_str() << std::endl;
    }
    BigInt(const mpz_class& val) : value(val) {
        // std::cerr << "BigInt::BigInt(mpz_class=" << val.get_str(10) << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(const mpz_t val) : value(val) {
        mpz_class temp(val);
        // std::cerr << "BigInt::BigInt(mpz_t=" << temp.get_str(10) << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(long val) : value(val) {
        // std::cerr << "BigInt::BigInt(long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(unsigned long val) : value(val) {
        // std::cerr << "BigInt::BigInt(unsigned long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(int val) : value(val) {
        // std::cerr << "BigInt::BigInt(int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(unsigned int val) : value(val) {
        // std::cerr << "BigInt::BigInt(unsigned int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(const std::string& str, int base = 10) : value(str, base) {
        // std::cerr << "BigInt::BigInt(string=\"" << str << "\", base=" << base << ") -> " << get_decimal_str() << std::endl;
    }
    BigInt(const char* str, int base = 10) : value(str, base) {
        // std::cerr << "BigInt::BigInt(char*=\"" << str << "\", base=" << base << ") -> " << get_decimal_str() << std::endl;
    }
    
    // Constructor from const BIGNUM*
    BigInt(const BIGNUM* bn) {
        if (bn == nullptr) {
            value = 0;
        } else {
            // Get the size in bytes
            int num_bytes = BN_num_bytes(bn);
            if (num_bytes == 0) {
                value = 0;
            } else {
                // Allocate buffer for binary representation
                std::vector<unsigned char> bytes(num_bytes);
                BN_bn2bin(bn, bytes.data());
                
                // Import into mpz_class (mpz_import expects big-endian format)
                // Parameters: count, order, size, endian, nails, op
                mpz_import(value.get_mpz_t(), num_bytes, 1, sizeof(unsigned char), 0, 0, bytes.data());
                
                // Handle sign (BN_bn2bin returns absolute value, so we need to check sign separately)
                if (BN_is_negative(bn)) {
                    value = -value;
                }
            }
        }
        // std::cerr << "BigInt::BigInt(BIGNUM*) -> " << get_decimal_str() << std::endl;
    }

    BigInt(const FMPZ &fmpz) {
        fmpz_get_mpz(value.get_mpz_t(), fmpz.get_fmpz());
        // std::cerr << "BigInt::BigInt(FMPZ=" << fmpz.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
    }
    
    // Copy constructor
    BigInt(const BigInt& other) : value(other.value) {
        // std::cerr << "BigInt::BigInt(BigInt=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
    }
    
    // Assignment operator
    BigInt& operator=(const BigInt& other) {
        if (this != &other) {
            value = other.value;
        }
        // std::cerr << "BigInt::operator=(BigInt=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    // Assignment from other types
    BigInt& operator=(const mpz_class& val) {
        value = val;
        // std::cerr << "BigInt::operator=(mpz_class=" << val.get_str(10) << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    BigInt& operator=(long val) {
        value = val;
        // std::cerr << "BigInt::operator=(long=" << val << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    BigInt& operator=(const std::string& str) {
        value = mpz_class(str);
        // std::cerr << "BigInt::operator=(string=\"" << str << "\") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Arithmetic operators
    BigInt operator+(const BigInt& other) const {
        BigInt result;
        result.value = value + other.value;
        // std::cerr << "BigInt::operator+(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator-(const BigInt& other) const {
        BigInt result;
        result.value = value - other.value;
        // std::cerr << "BigInt::operator-(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator*(const BigInt& other) const {
        BigInt result;
        result.value = value * other.value;
        // std::cerr << "BigInt::operator*(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator/(const BigInt& other) const {
        if (other.value == 0) {
            fprintf(stderr, "Error: Division by zero\n");
            abort();
        }
        BigInt result;
        result.value = value / other.value;
        // std::cerr << "BigInt::operator/(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator%(const BigInt& other) const {
        if (other.value == 0) {
            fprintf(stderr, "Error: Modulo by zero\n");
            abort();
        }
        BigInt result;
        result.value = value % other.value;
        // std::cerr << "BigInt::operator%(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Unary operators
    BigInt operator-() const {
        BigInt result;
        result.value = -value;
        // std::cerr << "BigInt::operator-(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator+() const {
        // std::cerr << "BigInt::operator+(this=" << get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    // Compound assignment operators
    BigInt& operator+=(const BigInt& other) {
        value += other.value;
        // std::cerr << "BigInt::operator+=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator-=(const BigInt& other) {
        value -= other.value;
        // std::cerr << "BigInt::operator-=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator*=(const BigInt& other) {
        value *= other.value;
        // std::cerr << "BigInt::operator*=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator/=(const BigInt& other) {
        if (other.value == 0) {
            fprintf(stderr, "Error: Division by zero\n");
            abort();
        }
        value /= other.value;
        // std::cerr << "BigInt::operator/=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator%=(const BigInt& other) {
        if (other.value == 0) {
            fprintf(stderr, "Error: Modulo by zero\n");
            abort();
        }
        value %= other.value;
        // std::cerr << "BigInt::operator%=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Comparison operators
    bool operator<(const BigInt& other) const {
        bool result = value < other.value;
        // std::cerr << "BigInt::operator<(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>(const BigInt& other) const {
        bool result = value > other.value;
        // std::cerr << "BigInt::operator>(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator<=(const BigInt& other) const {
        bool result = value <= other.value;
        // std::cerr << "BigInt::operator<=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>=(const BigInt& other) const {
        bool result = value >= other.value;
        // std::cerr << "BigInt::operator>=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator==(const BigInt& other) const {
        bool result = value == other.value;
        // std::cerr << "BigInt::operator==(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator!=(const BigInt& other) const {
        bool result = value != other.value;
        // std::cerr << "BigInt::operator!=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }

    // Bitwise operators
    BigInt operator<<(int shift) const {
        BigInt result;
        result.value = value << shift;
        // std::cerr << "BigInt::operator<<(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator>>(int shift) const {
        BigInt result;
        result.value = value >> shift;
        // std::cerr << "BigInt::operator>>(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator&(const BigInt& other) const {
        BigInt result;
        result.value = value & other.value;
        // std::cerr << "BigInt::operator&(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator|(const BigInt& other) const {
        BigInt result;
        result.value = value | other.value;
        // std::cerr << "BigInt::operator|(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator^(const BigInt& other) const {
        BigInt result;
        result.value = value ^ other.value;
        // std::cerr << "BigInt::operator^(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator~() const {
        BigInt result;
        result.value = ~value;
        // std::cerr << "BigInt::operator~(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Compound bitwise assignment operators
    BigInt& operator<<=(int shift) {
        value <<= shift;
        // std::cerr << "BigInt::operator<<=(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator>>=(int shift) {
        value >>= shift;
        // std::cerr << "BigInt::operator>>=(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator&=(const BigInt& other) {
        value &= other.value;
        // std::cerr << "BigInt::operator&=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator|=(const BigInt& other) {
        value |= other.value;
        // std::cerr << "BigInt::operator|=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator^=(const BigInt& other) {
        value ^= other.value;
        // std::cerr << "BigInt::operator^=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Power method with modular exponentiation
    // For negative exponents, computes modular inverse
    BigInt pow(const BigInt& exponent, const BigInt& modulus = BigInt(0)) const {
        BigInt result;
        if (modulus == 0) {
            // Regular exponentiation without modulus
            if (exponent < 0) {
                fprintf(stderr, "Error: Negative exponent not supported for non-modular exponentiation\n");
                abort();
            }
            mpz_pow_ui(result.value.get_mpz_t(), value.get_mpz_t(), mpz_get_ui(exponent.value.get_mpz_t()));
        } else {
            // Modular exponentiation
            if (exponent >= 0) {
                mpz_powm(result.value.get_mpz_t(), value.get_mpz_t(), 
                        exponent.value.get_mpz_t(), modulus.value.get_mpz_t());
            } else {
                // For negative exponents, compute modular inverse first
                BigInt abs_exp = -exponent;
                BigInt inverse;
                if (mpz_invert(inverse.value.get_mpz_t(), value.get_mpz_t(), modulus.value.get_mpz_t()) == 0) {
                    std::cerr << "Error: Modular inverse does not exist for " << get_decimal_str() << " and " << modulus.get_decimal_str() << std::endl;
                    fprintf(stderr, "Error: Modular inverse does not exist\n");
                    abort();
                }
                mpz_powm(result.value.get_mpz_t(), inverse.value.get_mpz_t(), 
                        abs_exp.value.get_mpz_t(), modulus.value.get_mpz_t());
            }
        }
        // std::cerr << "BigInt::pow(this=" << get_decimal_str() << ", exponent=" << exponent.get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }

    // Utility methods
    std::string to_string(int base = 10) const {
        std::string result = value.get_str(base);
        // std::cerr << "BigInt::to_string(this=" << get_decimal_str() << ", base=" << base << ") -> \"" << result << "\"" << std::endl;
        return result;
    }
    
    long to_long() const {
        long result = value.get_si();
        // std::cerr << "BigInt::to_long(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    unsigned long to_ulong() const {
        unsigned long result = value.get_ui();
        // std::cerr << "BigInt::to_ulong(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    double to_double() const {
        double result = value.get_d();
        // std::cerr << "BigInt::to_double(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    int sign() const {
        int result = mpz_sgn(value.get_mpz_t());
        // std::cerr << "BigInt::sign(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    size_t bit_length() const {
        size_t result = mpz_sizeinbase(value.get_mpz_t(), 2);
        // std::cerr << "BigInt::bit_length(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    bool is_zero() const {
        bool result = value == 0;
        // std::cerr << "BigInt::is_zero(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool is_negative() const {
        bool result = value < 0;
        // std::cerr << "BigInt::is_negative(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool is_positive() const {
        bool result = value > 0;
        // std::cerr << "BigInt::is_positive(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    // GCD and LCM
    BigInt gcd(const BigInt& other) const {
        BigInt result;
        mpz_gcd(result.value.get_mpz_t(), value.get_mpz_t(), other.value.get_mpz_t());
        // std::cerr << "BigInt::gcd(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt lcm(const BigInt& other) const {
        BigInt result;
        mpz_lcm(result.value.get_mpz_t(), value.get_mpz_t(), other.value.get_mpz_t());
        // std::cerr << "BigInt::lcm(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Modular inverse
    BigInt mod_inverse(const BigInt& modulus) const {
        BigInt result;
        if (mpz_invert(result.value.get_mpz_t(), value.get_mpz_t(), modulus.value.get_mpz_t()) == 0) {
            std::cerr << "Error: Modular inverse does not exist for " << get_decimal_str() << " and " << modulus.get_decimal_str() << std::endl;
            fprintf(stderr, "Error: Modular inverse does not exist\n");
            abort();
        }
        // std::cerr << "BigInt::mod_inverse(this=" << get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Square root
    BigInt sqrt() const {
        if (value < 0) {
            fprintf(stderr, "Error: Square root of negative number\n");
            abort();
        }
        BigInt result;
        mpz_sqrt(result.value.get_mpz_t(), value.get_mpz_t());
        // std::cerr << "BigInt::sqrt(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Access to underlying mpz_class for advanced operations
    const mpz_class& get_mpz() const {
        return value;
    }
    
    mpz_class& get_mpz() {
        return value;
    }
    
    // Convert to BIGNUM
    void to_bn(BIGNUM *out) const {
        // std::cerr << "BigInt::to_bn(this=" << this << ") -> BIGNUM" << std::endl;
        if (out == nullptr) {
            fprintf(stderr, "Error: BIGNUM output pointer is null\n");
            abort();
        }
        
        // Handle zero case
        if (value == 0) {
            BN_zero(out);
            return;
        }
        
        // Get the sign
        bool is_negative = (value < 0);
        mpz_class abs_value = (is_negative) ? -value : value;
        
        // Export mpz_class to binary representation
        // mpz_export with nullptr allocates memory and returns pointer
        size_t count = 0;
        void *data = mpz_export(nullptr, &count, 1, sizeof(unsigned char), 0, 0, abs_value.get_mpz_t());
        
        if (count == 0) {
            // Shouldn't happen if value != 0, but handle it
            BN_zero(out);
            return;
        }
        
        if (data == nullptr) {
            fprintf(stderr, "Error: Failed to export mpz_class to binary\n");
            abort();
        }
        
        // Convert to BIGNUM
        // BN_bin2bn expects big-endian format, which mpz_export provides by default
        BIGNUM *result = BN_bin2bn(static_cast<const unsigned char*>(data), count, out);
        
        // Free the exported data (allocated by mpz_export)
        free(data);
        
        if (result == nullptr) {
            fprintf(stderr, "Error: Failed to convert mpz_class to BIGNUM\n");
            abort();
        }
        
        // Set the sign
        if (is_negative) {
            BN_set_negative(out, 1);
        }
        
        // std::cerr << "BigInt::to_bn(this=" << get_decimal_str() << ") -> BIGNUM" << std::endl;
    }

    void to_fmpz(FMPZ &fmpz) const {
        fmpz_set_mpz(fmpz.get_fmpz(), value.get_mpz_t());
    }
    
    // Random number generation
    static BigInt random(const BigInt& max) {
        static gmp_randclass rng(gmp_randinit_default);
        static bool seeded = false;
        if (!seeded) {
            rng.seed(time(nullptr));
            seeded = true;
        }
        BigInt result;
        result.value = rng.get_z_range(max.value);
        // std::cerr << "BigInt::random(max=" << max.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Stream operators
    friend std::ostream& operator<<(std::ostream& os, const BigInt& bi) {
        os << bi.value;
        // std::cerr << "operator<<(BigInt=" << bi.get_decimal_str() << ")" << std::endl;
        return os;
    }
    
    friend std::istream& operator>>(std::istream& is, BigInt& bi) {
        is >> bi.value;
        // std::cerr << "operator>>(BigInt=" << bi.get_decimal_str() << ")" << std::endl;
        return is;
    }
};

// Global operators for mixed types
inline BigInt operator+(long lhs, const BigInt& rhs) {
    BigInt result = BigInt(lhs) + rhs;
    // std::cerr << "operator+(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    return result;
}

inline BigInt operator-(long lhs, const BigInt& rhs) {
    BigInt result = BigInt(lhs) - rhs;
    // std::cerr << "operator-(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    return result;
}

inline BigInt operator*(long lhs, const BigInt& rhs) {
    BigInt result = BigInt(lhs) * rhs;
    // std::cerr << "operator*(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    return result;
}

inline BigInt operator/(long lhs, const BigInt& rhs) {
    BigInt result = BigInt(lhs) / rhs;
    // std::cerr << "operator/(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    return result;
}

inline BigInt operator%(long lhs, const BigInt& rhs) {
    BigInt result = BigInt(lhs) % rhs;
    // std::cerr << "operator%(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    return result;
}

inline bool operator<(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) < rhs;
    // std::cerr << "operator<(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}

inline bool operator>(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) > rhs;
    // std::cerr << "operator>(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}

inline bool operator<=(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) <= rhs;
    // std::cerr << "operator<=(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}

inline bool operator>=(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) >= rhs;
    // std::cerr << "operator>=(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}

inline bool operator==(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) == rhs;
    // std::cerr << "operator==(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}

inline bool operator!=(long lhs, const BigInt& rhs) {
    bool result = BigInt(lhs) != rhs;
    // std::cerr << "operator!=(long=" << lhs << ", BigInt=" << rhs.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
    return result;
}
}

#endif // GMP_WRAPPER_HPP
