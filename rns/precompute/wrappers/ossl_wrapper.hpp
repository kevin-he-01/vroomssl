#ifndef OSSL_WRAPPER_HPP
#define OSSL_WRAPPER_HPP

// #pragma message ("Using OpenSSL BigInt wrapper")

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
// Note: This file uses internal BoringSSL functions and must be compiled
// as part of the BoringSSL build with access to internal headers
#include "../../../crypto/fipsmodule/bn/internal.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <vector>
#include <functional>
#include <climits>

class BigInt {
private:
    BIGNUM* value;

public:
    // Helper to get BN_CTX (thread-local would be better, but this works)
    static BN_CTX* get_ctx() {
        static thread_local BN_CTX* ctx = nullptr;
        if (ctx == nullptr) {
            ctx = BN_CTX_new();
        }
        return ctx;
    }

    // Helper to get decimal string representation for logging
    std::string get_decimal_str() const {
        char* str = BN_bn2dec(value);
        if (str == nullptr) {
            return "0";
        }
        std::string result(str);
        OPENSSL_free(str);
        return result;
    }

    std::string get_hex_str() const {
        char* str = BN_bn2hex(value);
        if (str == nullptr) {
            return "0";
        }
        std::string result(str);
        OPENSSL_free(str);
        return result;
    }
    
    // Helper for bitwise operations - convert to bytes, operate, convert back
    static BIGNUM* bitwise_op(const BIGNUM* a, const BIGNUM* b, 
                              std::function<uint8_t(uint8_t, uint8_t)> op) {
        int a_len = BN_num_bytes(a);
        int b_len = BN_num_bytes(b);
        int max_len = (a_len > b_len) ? a_len : b_len;
        
        std::vector<uint8_t> a_bytes(max_len, 0);
        std::vector<uint8_t> b_bytes(max_len, 0);
        std::vector<uint8_t> result_bytes(max_len);
        
        BN_bn2bin(a, a_bytes.data() + (max_len - a_len));
        BN_bn2bin(b, b_bytes.data() + (max_len - b_len));
        
        // Handle sign extension for negative numbers
        if (BN_is_negative(a)) {
            uint8_t sign_byte = 0xFF;
            for (int i = 0; i < max_len - a_len; i++) {
                a_bytes[i] = sign_byte;
            }
        }
        if (BN_is_negative(b)) {
            uint8_t sign_byte = 0xFF;
            for (int i = 0; i < max_len - b_len; i++) {
                b_bytes[i] = sign_byte;
            }
        }
        
        for (int i = 0; i < max_len; i++) {
            result_bytes[i] = op(a_bytes[i], b_bytes[i]);
        }
        
        BIGNUM* result = BN_bin2bn(result_bytes.data(), max_len, nullptr);
        return result;
    }

public:
    // Constructors
    BigInt() : value(BN_new()) {
        BN_zero(value);
        // std::cerr << "BigInt::BigInt() -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(long val) : value(BN_new()) {
        if (val >= 0) {
            BN_set_word(value, static_cast<unsigned long>(val));
        } else {
            BN_set_word(value, static_cast<unsigned long>(-val));
            BN_set_negative(value, 1);
        }
        // std::cerr << "BigInt::BigInt(long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(unsigned long val) : value(BN_new()) {
        BN_set_word(value, val);
        // std::cerr << "BigInt::BigInt(unsigned long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(int val) : value(BN_new()) {
        if (val >= 0) {
            BN_set_word(value, static_cast<unsigned long>(val));
        } else {
            BN_set_word(value, static_cast<unsigned long>(-val));
            BN_set_negative(value, 1);
        }
        // std::cerr << "BigInt::BigInt(int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(unsigned int val) : value(BN_new()) {
        BN_set_word(value, static_cast<unsigned long>(val));
        // std::cerr << "BigInt::BigInt(unsigned int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(const std::string& str, int base = 10) : value(BN_new()) {
        if (base == 10) {
            if (BN_dec2bn(&value, str.c_str()) == 0) {
                BN_free(value);
                fprintf(stderr, "Error: Invalid decimal string\n");
                abort();
            }
        } else if (base == 16) {
            if (BN_hex2bn(&value, str.c_str()) == 0) {
                BN_free(value);
                fprintf(stderr, "Error: Invalid hexadecimal string\n");
                abort();
            }
        } else if (base == 2) {
            BN_zero(value);
            for (char c : str) {
                if (c == '0' || c == '1') {
                    BN_lshift1(value, value);
                    if (c == '1') {
                        BN_add_word(value, 1);
                    }
                } else {
                    BN_free(value);
                    fprintf(stderr, "Error: Invalid binary string\n");
                    abort();
                }
            }
        } else if (base == 8) {
            // Parse octal manually
            BN_zero(value);
            for (char c : str) {
                if (c >= '0' && c <= '7') {
                    BN_mul_word(value, 8);
                    BN_add_word(value, c - '0');
                } else {
                    BN_free(value);
                    fprintf(stderr, "Error: Invalid octal string\n");
                    abort();
                }
            }
        } else {
            BN_free(value);
            fprintf(stderr, "Error: Unsupported base for string conversion\n");
            abort();
        }
        // std::cerr << "BigInt::BigInt(string=\"" << str << "\", base=" << base << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(const char* str, int base = 10) : value(BN_new()) {
        if (base == 10) {
            if (BN_dec2bn(&value, str) == 0) {
                BN_free(value);
                fprintf(stderr, "Error: Invalid decimal string\n");
                abort();
            }
        } else if (base == 16) {
            if (BN_hex2bn(&value, str) == 0) {
                BN_free(value);
                fprintf(stderr, "Error: Invalid hexadecimal string\n");
                abort();
            }
        } else if (base == 2) {
            BN_zero(value);
            std::string s(str);
            for (char c : s) {
                if (c == '0' || c == '1') {
                    BN_lshift1(value, value);
                    if (c == '1') {
                        BN_add_word(value, 1);
                    }
                } else {
                    BN_free(value);
                    fprintf(stderr, "Error: Invalid binary string\n");
                    abort();
                }
            }
        } else if (base == 8) {
            // Parse octal manually
            BN_zero(value);
            std::string s(str);
            for (char c : s) {
                if (c >= '0' && c <= '7') {
                    BN_mul_word(value, 8);
                    BN_add_word(value, c - '0');
                } else {
                    BN_free(value);
                    fprintf(stderr, "Error: Invalid octal string\n");
                    abort();
                }
            }
        } else {
            BN_free(value);
            fprintf(stderr, "Error: Unsupported base for string conversion\n");
            abort();
        }
        // std::cerr << "BigInt::BigInt(char*=\"" << str << "\", base=" << base << ") -> " << get_decimal_str() << std::endl;
    }
    
    // Copy constructor
    BigInt(const BigInt& other) : value(BN_new()) {
        BN_copy(value, other.value);
        // std::cerr << "BigInt::BigInt(BigInt=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
    }
    
    // Constructor from const BIGNUM*
    BigInt(const BIGNUM* bn) : value(BN_new()) {
        if (bn == nullptr) {
            BN_zero(value);
        } else {
            BN_copy(value, bn);
        }
        // std::cerr << "BigInt::BigInt(BIGNUM*) -> " << get_decimal_str() << std::endl;
    }
    
    // Destructor
    ~BigInt() {
        BN_free(value);
    }
    
    // Assignment operator
    BigInt& operator=(const BigInt& other) {
        if (this != &other) {
            BN_copy(value, other.value);
        }
        // std::cerr << "BigInt::operator=(BigInt=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    inline const BIGNUM* get_value() const {
        return value;
    }
    
    inline BIGNUM* get_value() {
        return value;
    }
    
    // Assignment from other types
    BigInt& operator=(long val) {
        if (val >= 0) {
            BN_set_word(value, static_cast<unsigned long>(val));
            BN_set_negative(value, 0);
        } else {
            BN_set_word(value, static_cast<unsigned long>(-val));
            BN_set_negative(value, 1);
        }
        // std::cerr << "BigInt::operator=(long=" << val << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator=(const std::string& str) {
        if (BN_dec2bn(&value, str.c_str()) == 0) {
            fprintf(stderr, "Error: Invalid decimal string\n");
            abort();
        }
        // std::cerr << "BigInt::operator=(string=\"" << str << "\") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Arithmetic operators
    BigInt operator+(const BigInt& other) const {
        BigInt result;
        BN_add(result.value, value, other.value);
        // std::cerr << "BigInt::operator+(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator-(const BigInt& other) const {
        BigInt result;
        BN_sub(result.value, value, other.value);
        // std::cerr << "BigInt::operator-(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator*(const BigInt& other) const {
        BigInt result;
        BN_mul(result.value, value, other.value, get_ctx());
        // std::cerr << "BigInt::operator*(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator/(const BigInt& other) const {
        if (BN_is_zero(other.value)) {
            fprintf(stderr, "Error: Division by zero\n");
            abort();
        }
        BigInt result;
        BN_div(result.value, nullptr, value, other.value, get_ctx());
        // std::cerr << "BigInt::operator/(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator%(const BigInt& other) const {
        if (BN_is_zero(other.value)) {
            fprintf(stderr, "Error: Modulo by zero\n");
            abort();
        }
        BigInt result;
        BN_mod(result.value, value, other.value, get_ctx());
        // std::cerr << "BigInt::operator%(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Unary operators
    BigInt operator-() const {
        BigInt result(*this);
        BN_set_negative(result.value, !BN_is_negative(result.value));
        // std::cerr << "BigInt::operator-(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator+() const {
        // std::cerr << "BigInt::operator+(this=" << get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    // Compound assignment operators
    BigInt& operator+=(const BigInt& other) {
        BN_add(value, value, other.value);
        // std::cerr << "BigInt::operator+=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator-=(const BigInt& other) {
        BN_sub(value, value, other.value);
        // std::cerr << "BigInt::operator-=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator*=(const BigInt& other) {
        BIGNUM* temp = BN_new();
        BN_mul(temp, value, other.value, get_ctx());
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator*=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator/=(const BigInt& other) {
        if (BN_is_zero(other.value)) {
            fprintf(stderr, "Error: Division by zero\n");
            abort();
        }
        BIGNUM* temp = BN_new();
        BN_div(temp, nullptr, value, other.value, get_ctx());
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator/=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator%=(const BigInt& other) {
        if (BN_is_zero(other.value)) {
            fprintf(stderr, "Error: Modulo by zero\n");
            abort();
        }
        BIGNUM* temp = BN_new();
        BN_mod(temp, value, other.value, get_ctx());
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator%=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Comparison operators
    bool operator<(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) < 0;
        // std::cerr << "BigInt::operator<(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) > 0;
        // std::cerr << "BigInt::operator>(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator<=(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) <= 0;
        // std::cerr << "BigInt::operator<=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>=(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) >= 0;
        // std::cerr << "BigInt::operator>=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator==(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) == 0;
        // std::cerr << "BigInt::operator==(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator!=(const BigInt& other) const {
        bool result = BN_cmp(value, other.value) != 0;
        // std::cerr << "BigInt::operator!=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }

    // Bitwise operators
    BigInt operator<<(int shift) const {
        BigInt result;
        BN_lshift(result.value, value, shift);
        // std::cerr << "BigInt::operator<<(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator>>(int shift) const {
        BigInt result;
        BN_rshift(result.value, value, shift);
        // std::cerr << "BigInt::operator>>(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator&(const BigInt& other) const {
        BigInt result;
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a & b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator&(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator|(const BigInt& other) const {
        BigInt result;
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a | b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator|(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator^(const BigInt& other) const {
        BigInt result;
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a ^ b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator^(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator~() const {
        // For bitwise NOT, we need to work with a fixed bit length
        // We'll use the bit length of the number
        int bits = BN_num_bits(value);
        BigInt result;
        if (bits == 0) {
            // NOT of 0 is -1 (all bits set)
            result = BigInt(-1);
        } else {
            // Create a mask of all 1s with the same bit length
            for (int i = 0; i < bits; i++) {
                BN_set_bit(result.value, i);
            }
            // XOR with mask to get NOT (using bitwise_op)
            BIGNUM* temp = bitwise_op(result.value, value, [](uint8_t a, uint8_t b) { return a ^ b; });
            BN_copy(result.value, temp);
            BN_free(temp);
            // For negative numbers, we need to handle sign extension
            // This is a simplified version - full two's complement NOT would be more complex
            if (BN_is_negative(value)) {
                BN_set_negative(result.value, 0);
            } else {
                // If original was positive and result has all bits set, make it negative
                BigInt all_ones;
                for (int i = 0; i < bits; i++) {
                    BN_set_bit(all_ones.value, i);
                }
                if (BN_cmp(result.value, all_ones.value) == 0) {
                    BN_set_negative(result.value, 1);
                    BN_sub_word(result.value, 1);
                }
            }
        }
        // std::cerr << "BigInt::operator~(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }

    void to_bn(BIGNUM* out) const {
        BN_copy(out, value);
        // std::cerr << "BigInt::to_bn(this=" << get_decimal_str() << ") -> BIGNUM" << std::endl;
    }
    
    // Compound bitwise assignment operators
    BigInt& operator<<=(int shift) {
        BN_lshift(value, value, shift);
        // std::cerr << "BigInt::operator<<=(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator>>=(int shift) {
        BN_rshift(value, value, shift);
        // std::cerr << "BigInt::operator>>=(this=" << get_decimal_str() << ", shift=" << shift << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator&=(const BigInt& other) {
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a & b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator&=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator|=(const BigInt& other) {
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a | b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator|=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator^=(const BigInt& other) {
        BIGNUM* temp = bitwise_op(value, other.value, [](uint8_t a, uint8_t b) { return a ^ b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator^=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Power method with modular exponentiation
    BigInt pow(const BigInt& exponent, const BigInt& modulus = BigInt(0)) const {
        BigInt result;
        if (BN_is_zero(modulus.value)) {
            // Regular exponentiation without modulus
            if (BN_is_negative(exponent.value)) {
                fprintf(stderr, "Error: Negative exponent not supported for non-modular exponentiation\n");
                abort();
            }
            // BN doesn't have direct pow_ui, so we need to implement it
            // For now, convert exponent to unsigned long (limitation)
            unsigned long exp = BN_get_word(exponent.value);
            if (exp == ULONG_MAX && !BN_is_zero(exponent.value)) {
                fprintf(stderr, "Error: Exponent too large for non-modular exponentiation\n");
                abort();
            }
            
            result = BigInt(1);
            BigInt base(*this);
            while (exp > 0) {
                if (exp & 1) {
                    result *= base;
                }
                base *= base;
                exp >>= 1;
            }
        } else {
            // Modular exponentiation
            if (!BN_is_negative(exponent.value)) {
                BN_mod_exp(result.value, value, exponent.value, modulus.value, get_ctx());
            } else {
                // For negative exponents, compute modular inverse first
                BigInt abs_exp = -exponent;
                BigInt inverse;
                if (BN_mod_inverse(inverse.value, value, modulus.value, get_ctx()) == nullptr) {
                    // std::cerr << "Error: Modular inverse does not exist for " << get_decimal_str() << " and " << modulus.get_decimal_str() << std::endl;
                    fprintf(stderr, "Error: Modular inverse does not exist\n");
                    abort();
                }
                BN_mod_exp(result.value, inverse.value, abs_exp.value, modulus.value, get_ctx());
            }
        }
        // std::cerr << "BigInt::pow(this=" << get_decimal_str() << ", exponent=" << exponent.get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }

    // Utility methods
    std::string to_string(int base = 10) const {
        std::string result;
        if (base == 10) {
            char* str = BN_bn2dec(value);
            if (str == nullptr) {
                fprintf(stderr, "Error: Failed to convert to decimal string\n");
                abort();
            }
            result = std::string(str);
            OPENSSL_free(str);
        } else if (base == 16) {
            char* str = BN_bn2hex(value);
            if (str == nullptr) {
                fprintf(stderr, "Error: Failed to convert to hexadecimal string\n");
                abort();
            }
            result = std::string(str);
            OPENSSL_free(str);
            // Convert to lowercase for consistency
            for (char& c : result) {
                if (c >= 'A' && c <= 'F') {
                    c = c - 'A' + 'a';
                }
            }
        } else if (base == 2) {
            // Convert to binary string
            int bits = BN_num_bits(value);
            if (bits == 0) {
                result = "0";
            } else {
                for (int i = bits - 1; i >= 0; i--) {
                    result += (BN_is_bit_set(value, i) ? '1' : '0');
                }
            }
        } else {
            fprintf(stderr, "Error: Unsupported base for string conversion\n");
            abort();
        }
        // std::cerr << "BigInt::to_string(this=" << get_decimal_str() << ", base=" << base << ") -> \"" << result << "\"" << std::endl;
        return result;
    }
    
    long to_long() const {
        long result;
        if (BN_is_negative(value)) {
            result = -static_cast<long>(BN_get_word(value));
        } else {
            result = static_cast<long>(BN_get_word(value));
        }
        // std::cerr << "BigInt::to_long(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    unsigned long to_ulong() const {
        // Extract lowest 64 bits (modulo 2^64) to match GMP behavior
        // BN_get_word only works for values that fit in unsigned long
        // For larger values, we need to extract the lowest 64 bits manually
        unsigned long result = 0;
        int num_bytes = BN_num_bytes(value);
        if (num_bytes == 0) {
            result = 0;
        } else if (num_bytes <= 8) {
            // Value fits in 64 bits, use BN_get_word directly
            result = BN_get_word(value);
        } else {
            // Value is larger than 64 bits, extract lowest 8 bytes
            std::vector<unsigned char> bytes(num_bytes);
            BN_bn2bin(value, bytes.data());
            // BN_bn2bin returns big-endian, so the least significant bytes are at the end
            // Extract the last 8 bytes (lowest 64 bits)
            for (int i = 0; i < 8; i++) {
                int byte_idx = num_bytes - 8 + i;
                result |= static_cast<unsigned long>(bytes[byte_idx]) << ((7 - i) * 8);
            }
        }
        // std::cerr << "BigInt::to_ulong(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    double to_double() const {
        // Convert via string for accuracy
        std::string str = to_string();
        double result = std::stod(str);
        // std::cerr << "BigInt::to_double(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    int sign() const {
        int result;
        if (BN_is_zero(value)) {
            result = 0;
        } else {
            result = BN_is_negative(value) ? -1 : 1;
        }
        // std::cerr << "BigInt::sign(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    size_t bit_length() const {
        int bits = BN_num_bits(value);
        // Match GMP behavior: return 1 for zero
        size_t result = (bits == 0) ? 1 : bits;
        // std::cerr << "BigInt::bit_length(this=" << get_decimal_str() << ") -> " << result << std::endl;
        return result;
    }
    
    bool is_zero() const {
        bool result = BN_is_zero(value);
        // std::cerr << "BigInt::is_zero(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool is_negative() const {
        bool result = BN_is_negative(value);
        // std::cerr << "BigInt::is_negative(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool is_positive() const {
        bool result = !BN_is_zero(value) && !BN_is_negative(value);
        // std::cerr << "BigInt::is_positive(this=" << get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    // GCD and LCM
    BigInt gcd(const BigInt& other) const {
        BigInt result;
        BN_gcd(result.value, value, other.value, get_ctx());
        // std::cerr << "BigInt::gcd(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt lcm(const BigInt& other) const {
        BigInt result;
        BigInt gcd_val = gcd(other);
        if (BN_is_zero(gcd_val.value)) {
            BN_zero(result.value);
        } else {
            BigInt temp = (*this) * other;
            BN_div(result.value, nullptr, temp.value, gcd_val.value, get_ctx());
            // Make result positive
            BN_set_negative(result.value, 0);
        }
        // std::cerr << "BigInt::lcm(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Modular inverse
    BigInt mod_inverse(const BigInt& modulus) const {
        BigInt result;
        if (BN_mod_inverse(result.value, value, modulus.value, get_ctx()) == nullptr) {
            fprintf(stderr, "Error: Modular inverse does not exist\n");
            abort();
        }
        // std::cerr << "BigInt::mod_inverse(this=" << get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Square root (using binary search since BN_sqrt doesn't exist)
    BigInt sqrt() const {
        if (BN_is_negative(value)) {
            fprintf(stderr, "Error: Square root of negative number\n");
            abort();
        }
        BigInt result;
        if (BN_is_zero(value)) {
            result = BigInt(0);
        } else if (BN_is_one(value)) {
            result = BigInt(1);
        } else {
            // Binary search for square root
            BigInt low(1);
            BigInt high(*this);
            
            while (low <= high) {
                BigInt mid = (low + high) / BigInt(2);
                BigInt square = mid * mid;
                
                if (square == *this) {
                    result = mid;
                    break;
                } else if (square < *this) {
                    result = mid;
                    low = mid + BigInt(1);
                } else {
                    high = mid - BigInt(1);
                }
            }
        }
        // std::cerr << "BigInt::sqrt(this=" << get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Access to underlying BIGNUM for advanced operations
    const BIGNUM* get_mpz() const {
        // std::cerr << "BigInt::get_mpz() const (this=" << get_decimal_str() << ")" << std::endl;
        return value;
    }
    
    BIGNUM* get_mpz() {
        // std::cerr << "BigInt::get_mpz() (this=" << get_decimal_str() << ")" << std::endl;
        return value;
    }
    
    // Random number generation
    static BigInt random(const BigInt& max) {
        BigInt result;
        // BN_rand_range generates a random number in [0, max)
        if (BN_rand_range(result.value, max.value) == 0) {
            fprintf(stderr, "Error: Random number generation failed\n");
            abort();
        }
        // std::cerr << "BigInt::random(max=" << max.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Stream operators
    friend std::ostream& operator<<(std::ostream& os, const BigInt& bi) {
        os << bi.to_string();
        // std::cerr << "operator<<(BigInt=" << bi.get_decimal_str() << ")" << std::endl;
        return os;
    }
    
    friend std::istream& operator>>(std::istream& is, BigInt& bi) {
        std::string str;
        is >> str;
        if (BN_dec2bn(&bi.value, str.c_str()) == 0) {
            is.setstate(std::ios::failbit);
        }
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

#endif // OSSL_WRAPPER_HPP
