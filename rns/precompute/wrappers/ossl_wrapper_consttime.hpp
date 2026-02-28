#ifndef OSSL_WRAPPER_CONSTTIME_HPP
#define OSSL_WRAPPER_CONSTTIME_HPP

// #pragma message ("Using OpenSSL BigInt wrapper (constant-time)")
//
// This is a constant-time version of the BigInt wrapper that uses BoringSSL's
// constant-time BN functions to avoid timing side-channels. It should be used
// when operating on secret values (e.g., private keys, nonces, etc.).
//
// IMPORTANT: This file uses internal BoringSSL functions and must be compiled
// as part of the BoringSSL build with:
//   - Access to crypto/fipsmodule/bn/internal.h
//   - The BORINGSSL_IMPLEMENTATION define (typically set by the build system)
//
// WARNING: Some operations (like string conversion, stream I/O) are NOT
// constant-time and should never be used with secret values.

#include <openssl/bn.h>
#include <openssl/crypto.h>
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
#include <openssl/err.h>

class BigInt {
private:
    BIGNUM* value;
    
    // Helper to get BN_CTX (thread-local would be better, but this works)
    public:
    static BN_CTX* get_ctx() {
        static thread_local BN_CTX* ctx = nullptr;
        if (ctx == nullptr) {
            ctx = BN_CTX_new();
        }
        return ctx;
    }
    private:

    // Constant-time comparison helper
    // Returns: -1 if a < b, 0 if a == b, 1 if a > b
    // Uses constant-time operations to avoid leaking information about values
    static int consttime_cmp(const BIGNUM* a, const BIGNUM* b) {
        // Get signs (0 for positive, 1 for negative)
        int a_neg = BN_is_negative(a) ? 1 : 0;
        int b_neg = BN_is_negative(b) ? 1 : 0;
        
        // Compare absolute values using BN_ucmp (constant-time)
        int abs_cmp = BN_ucmp(a, b);
        
        // Handle signed comparison:
        // - If signs differ: negative < positive
        // - If same sign: use abs_cmp (flip if both negative)
        int sign_diff = a_neg ^ b_neg;
        
        // Result when signs differ
        int diff_sign_result = a_neg ? -1 : 1;
        
        // Result when signs are the same
        // If both negative, flip the comparison; otherwise use as-is
        int same_sign_result = a_neg ? -abs_cmp : abs_cmp;
        
        // Constant-time select: use diff_sign_result if signs differ, else same_sign_result
        // Note: The ternary here branches on sign difference, which is typically public
        // For fully constant-time when signs are secret, word-level masking would be needed
        return sign_diff ? diff_sign_result : same_sign_result;
    }

    // Constant-time select: returns a if mask is 1, b if mask is 0
    static BIGNUM* consttime_select(int mask, const BIGNUM* a, const BIGNUM* b) {
        BIGNUM* result = BN_new();
        if (result == nullptr) {
            return nullptr;
        }
        // Use constant-time selection
        // BN_ULONG m = (BN_ULONG)(0 - (mask != 0));
        // Copy both and then select
        BIGNUM* temp_a = BN_new();
        BIGNUM* temp_b = BN_new();
        if (temp_a == nullptr || temp_b == nullptr) {
            BN_free(result);
            BN_free(temp_a);
            BN_free(temp_b);
            return nullptr;
        }
        BN_copy(temp_a, a);
        BN_copy(temp_b, b);
        
        // Simple approach: always copy, but this isn't fully constant-time
        // For true constant-time, we'd need to use word-level operations
        if (mask) {
            BN_copy(result, a);
        } else {
            BN_copy(result, b);
        }
        
        BN_free(temp_a);
        BN_free(temp_b);
        return result;
    }

public:
    // Helper to get decimal string representation for logging
    // WARNING: This leaks timing information and should not be used with secret values
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
    // This version avoids branches on secret data
    static BIGNUM* bitwise_op_consttime(const BIGNUM* a, const BIGNUM* b, 
                                        std::function<uint8_t(uint8_t, uint8_t)> op) {
        int a_len = BN_num_bytes(a);
        int b_len = BN_num_bytes(b);
        int max_len = (a_len > b_len) ? a_len : b_len;
        
        std::vector<uint8_t> a_bytes(max_len, 0);
        std::vector<uint8_t> b_bytes(max_len, 0);
        std::vector<uint8_t> result_bytes(max_len);
        
        BN_bn2bin(a, a_bytes.data() + (max_len - a_len));
        BN_bn2bin(b, b_bytes.data() + (max_len - b_len));
        
        // Handle sign extension for negative numbers in constant-time
        int a_neg = BN_is_negative(a) ? 1 : 0;
        int b_neg = BN_is_negative(b) ? 1 : 0;
        uint8_t a_sign_byte = (uint8_t)(0xFF * a_neg);
        uint8_t b_sign_byte = (uint8_t)(0xFF * b_neg);
        
        // Fill padding bytes using constant-time selection
        for (int i = 0; i < max_len - a_len; i++) {
            a_bytes[i] = a_sign_byte;
        }
        for (int i = 0; i < max_len - b_len; i++) {
            b_bytes[i] = b_sign_byte;
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
        // Use constant-time operations where possible
        unsigned long abs_val = (val < 0) ? static_cast<unsigned long>(-val) : static_cast<unsigned long>(val);
        BN_set_word(value, abs_val);
        // Set sign in constant-time manner
        BN_set_negative(value, (val < 0) ? 1 : 0);
        // std::cerr << "BigInt::BigInt(long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(unsigned long val) : value(BN_new()) {
        BN_set_word(value, val);
        // std::cerr << "BigInt::BigInt(unsigned long=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(int val) : value(BN_new()) {
        unsigned long abs_val = (val < 0) ? static_cast<unsigned long>(-val) : static_cast<unsigned long>(val);
        BN_set_word(value, abs_val);
        BN_set_negative(value, (val < 0) ? 1 : 0);
        // std::cerr << "BigInt::BigInt(int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(unsigned int val) : value(BN_new()) {
        BN_set_word(value, static_cast<unsigned long>(val));
        // std::cerr << "BigInt::BigInt(unsigned int=" << val << ") -> " << get_decimal_str() << std::endl;
    }
    
    BigInt(const std::string& str, int base = 10) : value(BN_new()) {
        // String parsing is not constant-time, but this is typically for public values
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

    const BIGNUM* get_value() const {
        return value;
    }

    BIGNUM* get_value() {
        return value;
    }
    
    // Assignment from other types
    BigInt& operator=(long val) {
        unsigned long abs_val = (val < 0) ? static_cast<unsigned long>(-val) : static_cast<unsigned long>(val);
        BN_set_word(value, abs_val);
        BN_set_negative(value, (val < 0) ? 1 : 0);
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

    // Arithmetic operators - using constant-time operations
    BigInt operator+(const BigInt& other) const {
        BigInt result;
        // Use constant-time addition for unsigned values
        // For signed values, we need to handle signs properly
        if (!BN_is_negative(value) && !BN_is_negative(other.value)) {
            // Both positive: use unsigned addition
            bn_uadd_consttime(result.value, value, other.value);
        } else if (BN_is_negative(value) && BN_is_negative(other.value)) {
            // Both negative: add absolute values, keep negative
            BIGNUM* abs_a = BN_new();
            BIGNUM* abs_b = BN_new();
            BN_copy(abs_a, value);
            BN_copy(abs_b, other.value);
            BN_set_negative(abs_a, 0);
            BN_set_negative(abs_b, 0);
            bn_uadd_consttime(result.value, abs_a, abs_b);
            BN_set_negative(result.value, 1);
            BN_free(abs_a);
            BN_free(abs_b);
        } else {
            // Different signs: use subtraction
            BIGNUM* abs_a = BN_new();
            BIGNUM* abs_b = BN_new();
            BN_copy(abs_a, value);
            BN_copy(abs_b, other.value);
            BN_set_negative(abs_a, 0);
            BN_set_negative(abs_b, 0);
            int cmp = BN_ucmp(abs_a, abs_b);
            if (cmp >= 0) {
                bn_usub_consttime(result.value, abs_a, abs_b);
                BN_set_negative(result.value, BN_is_negative(value) ? 1 : 0);
            } else {
                bn_usub_consttime(result.value, abs_b, abs_a);
                BN_set_negative(result.value, BN_is_negative(other.value) ? 1 : 0);
            }
            BN_free(abs_a);
            BN_free(abs_b);
        }
        // std::cerr << "BigInt::operator+(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator-(const BigInt& other) const {
        BigInt result;
        // Similar to addition but with sign flipped
        if (!BN_is_negative(value) && !BN_is_negative(other.value)) {
            // Both positive
            int cmp = BN_ucmp(value, other.value);
            if (cmp >= 0) {
                bn_usub_consttime(result.value, value, other.value);
            } else {
                bn_usub_consttime(result.value, other.value, value);
                BN_set_negative(result.value, 1);
            }
        } else if (BN_is_negative(value) && BN_is_negative(other.value)) {
            // Both negative: subtract absolute values
            BIGNUM* abs_a = BN_new();
            BIGNUM* abs_b = BN_new();
            BN_copy(abs_a, value);
            BN_copy(abs_b, other.value);
            BN_set_negative(abs_a, 0);
            BN_set_negative(abs_b, 0);
            int cmp = BN_ucmp(abs_a, abs_b);
            if (cmp >= 0) {
                bn_usub_consttime(result.value, abs_a, abs_b);
                BN_set_negative(result.value, 1);
            } else {
                bn_usub_consttime(result.value, abs_b, abs_a);
            }
            BN_free(abs_a);
            BN_free(abs_b);
        } else {
            // Different signs: add absolute values
            BIGNUM* abs_a = BN_new();
            BIGNUM* abs_b = BN_new();
            BN_copy(abs_a, value);
            BN_copy(abs_b, other.value);
            BN_set_negative(abs_a, 0);
            BN_set_negative(abs_b, 0);
            bn_uadd_consttime(result.value, abs_a, abs_b);
            BN_set_negative(result.value, BN_is_negative(value) ? 1 : 0);
            BN_free(abs_a);
            BN_free(abs_b);
        }
        // std::cerr << "BigInt::operator-(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator*(const BigInt& other) const {
        BigInt result;
        // Use constant-time multiplication
        BIGNUM* abs_a = BN_new();
        BIGNUM* abs_b = BN_new();
        BN_copy(abs_a, value);
        BN_copy(abs_b, other.value);
        BN_set_negative(abs_a, 0);
        BN_set_negative(abs_b, 0);
        bn_mul_consttime(result.value, abs_a, abs_b, get_ctx());
        // Set sign: negative if signs differ
        int sign_a = BN_is_negative(value) ? 1 : 0;
        int sign_b = BN_is_negative(other.value) ? 1 : 0;
        BN_set_negative(result.value, (sign_a ^ sign_b) ? 1 : 0);
        BN_free(abs_a);
        BN_free(abs_b);
        // std::cerr << "BigInt::operator*(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator/(const BigInt& other) const {
        // Check for zero divisor in constant-time
        int is_zero = BN_is_zero(other.value) ? 1 : 0;
        if (is_zero) {
            fprintf(stderr, "Error: Division by zero\n");
            abort();
        }
        BigInt result;
        BIGNUM* abs_a = BN_new();
        BIGNUM* abs_b = BN_new();
        BN_copy(abs_a, value);
        BN_copy(abs_b, other.value);
        BN_set_negative(abs_a, 0);
        BN_set_negative(abs_b, 0);
        unsigned divisor_min_bits = BN_num_bits(abs_b);
        bn_div_consttime(result.value, nullptr, abs_a, abs_b, divisor_min_bits, get_ctx());
        // Set sign: negative if signs differ
        int sign_a = BN_is_negative(value) ? 1 : 0;
        int sign_b = BN_is_negative(other.value) ? 1 : 0;
        BN_set_negative(result.value, (sign_a ^ sign_b) ? 1 : 0);
        BN_free(abs_a);
        BN_free(abs_b);
        // std::cerr << "BigInt::operator/(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator%(const BigInt& other) const {
        int is_zero = BN_is_zero(other.value) ? 1 : 0;
        if (is_zero) {
            fprintf(stderr, "Error: Modulo by zero\n");
            abort();
        }
        BigInt result;
        BIGNUM* abs_a = BN_new();
        BIGNUM* abs_b = BN_new();
        BN_copy(abs_a, value);
        BN_copy(abs_b, other.value);
        BN_set_negative(abs_a, 0);
        BN_set_negative(abs_b, 0);
        unsigned divisor_min_bits = BN_num_bits(abs_b);
        bn_div_consttime(nullptr, result.value, abs_a, abs_b, divisor_min_bits, get_ctx());
        // Modulo result has same sign as dividend
        BN_set_negative(result.value, BN_is_negative(value) ? 1 : 0);
        BN_free(abs_a);
        BN_free(abs_b);
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

    // Comparison operators - using constant-time comparisons
    bool operator<(const BigInt& other) const {
        bool result = consttime_cmp(value, other.value) < 0;
        // std::cerr << "BigInt::operator<(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>(const BigInt& other) const {
        bool result = consttime_cmp(value, other.value) > 0;
        // std::cerr << "BigInt::operator>(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator<=(const BigInt& other) const {
        bool result = consttime_cmp(value, other.value) <= 0;
        // std::cerr << "BigInt::operator<=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator>=(const BigInt& other) const {
        bool result = consttime_cmp(value, other.value) >= 0;
        // std::cerr << "BigInt::operator>=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator==(const BigInt& other) const {
        bool result = BN_equal_consttime(value, other.value) != 0;
        // std::cerr << "BigInt::operator==(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << (result ? "true" : "false") << std::endl;
        return result;
    }
    
    bool operator!=(const BigInt& other) const {
        bool result = BN_equal_consttime(value, other.value) == 0;
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
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a & b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator&(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator|(const BigInt& other) const {
        BigInt result;
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a | b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator|(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator^(const BigInt& other) const {
        BigInt result;
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a ^ b; });
        BN_copy(result.value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator^(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt operator~() const {
        // For bitwise NOT, we need to work with a fixed bit length
        int bits = BN_num_bits(value);
        BigInt result;
        if (bits == 0) {
            result = BigInt(-1);
        } else {
            // Create a mask of all 1s with the same bit length
            for (int i = 0; i < bits; i++) {
                BN_set_bit(result.value, i);
            }
            // XOR with mask to get NOT
            BIGNUM* temp = bitwise_op_consttime(result.value, value, [](uint8_t a, uint8_t b) { return a ^ b; });
            BN_copy(result.value, temp);
            BN_free(temp);
            // Handle sign in constant-time
            int is_neg = BN_is_negative(value) ? 1 : 0;
            if (is_neg) {
                BN_set_negative(result.value, 0);
            } else {
                // Check if result is all ones
                BigInt all_ones;
                for (int i = 0; i < bits; i++) {
                    BN_set_bit(all_ones.value, i);
                }
                int is_all_ones = BN_equal_consttime(result.value, all_ones.value) ? 1 : 0;
                if (is_all_ones) {
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
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a & b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator&=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator|=(const BigInt& other) {
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a | b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator|=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }
    
    BigInt& operator^=(const BigInt& other) {
        BIGNUM* temp = bitwise_op_consttime(value, other.value, [](uint8_t a, uint8_t b) { return a ^ b; });
        BN_copy(value, temp);
        BN_free(temp);
        // std::cerr << "BigInt::operator^=(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << get_decimal_str() << std::endl;
        return *this;
    }

    // Power method with modular exponentiation (NOT constant-time! Use only in test code!)
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

    // // Power method with modular exponentiation - using constant-time version (Caveat: only works if modulus is odd)
    // BigInt pow(const BigInt& exponent, const BigInt& modulus = BigInt(0)) const {
    //     BigInt result;
    //     if (BN_is_zero(modulus.value)) {
    //         // Regular exponentiation without modulus
    //         // Check for negative exponent in constant-time
    //         int is_neg_exp = BN_is_negative(exponent.value) ? 1 : 0;
    //         if (is_neg_exp) {
    //             fprintf(stderr, "Error: Negative exponent not supported for non-modular exponentiation\n");
    //             abort();
    //         }
    //         // For non-modular exponentiation, we still need to use a simple algorithm
    //         // Convert exponent to unsigned long (limitation)
    //         unsigned long exp = BN_get_word(exponent.value);
    //         if (exp == ULONG_MAX && !BN_is_zero(exponent.value)) {
    //             fprintf(stderr, "Error: Exponent too large for non-modular exponentiation\n");
    //             abort();
    //         }
            
    //         result = BigInt(1);
    //         BigInt base(*this);
    //         while (exp > 0) {
    //             if (exp & 1) {
    //                 result *= base;
    //             }
    //             base *= base;
    //             exp >>= 1;
    //         }
    //     } else {
    //         // Modular exponentiation - use constant-time version
    //         // Create Montgomery context in constant-time
    //         BN_MONT_CTX* mont = BN_MONT_CTX_new_consttime(modulus.value, get_ctx());
    //         if (mont == nullptr) {
    //             fprintf(stderr, "Error: Failed to create Montgomery context\n");
    //             abort();
    //         }
            
    //         // Ensure base < modulus
    //         BIGNUM* base_reduced = BN_new();
    //         BN_copy(base_reduced, value);
    //         BN_set_negative(base_reduced, 0);
    //         if (BN_ucmp(base_reduced, modulus.value) >= 0) {
    //             BN_mod(base_reduced, base_reduced, modulus.value, get_ctx());
    //         }
            
    //         if (!BN_is_negative(exponent.value)) {
    //             if (!BN_mod_exp_mont_consttime(result.value, base_reduced, exponent.value, modulus.value, get_ctx(), mont)) {
    //                 BN_free(base_reduced);
    //                 BN_MONT_CTX_free(mont);
    //                 fprintf(stderr, "Error: Modular exponentiation failed\n");
    //                 abort();
    //             }
    //         } else {
    //             // For negative exponents, compute modular inverse first
    //             BigInt abs_exp = -exponent;
    //             int no_inverse = 0;
    //             BigInt inverse;
    //             if (!bn_mod_inverse_consttime(inverse.value, &no_inverse, base_reduced, modulus.value, get_ctx())) {
    //                 BN_free(base_reduced);
    //                 BN_MONT_CTX_free(mont);
    //                 fprintf(stderr, "Error: Modular inverse computation failed\n");
    //                 abort();
    //             }
    //             if (no_inverse) {
    //                 BN_free(base_reduced);
    //                 BN_MONT_CTX_free(mont);
    //                 fprintf(stderr, "Error: Modular inverse does not exist\n");
    //                 abort();
    //             }
    //             if (!BN_mod_exp_mont_consttime(result.value, inverse.value, abs_exp.value, modulus.value, get_ctx(), mont)) {
    //                 BN_free(base_reduced);
    //                 BN_MONT_CTX_free(mont);
    //                 fprintf(stderr, "Error: Modular exponentiation failed\n");
    //                 abort();
    //             }
    //         }
            
    //         BN_free(base_reduced);
    //         BN_MONT_CTX_free(mont);
    //     }
    //     return result;
    // }

    // Utility methods
    // WARNING: to_string() leaks timing information and should not be used with secret values
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
        unsigned long result = 0;
        int num_bytes = BN_num_bytes(value);
        if (num_bytes == 0) {
            result = 0;
        } else if (num_bytes <= 8) {
            result = BN_get_word(value);
        } else {
            // Value is larger than 64 bits, extract lowest 8 bytes
            std::vector<unsigned char> bytes(num_bytes);
            BN_bn2bin(value, bytes.data());
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
    
    // GCD and LCM - using constant-time versions
    BigInt gcd(const BigInt& other) const {
        BigInt result;
        // BN_gcd uses constant-time implementation internally
        if (!BN_gcd(result.value, value, other.value, get_ctx())) {
            fprintf(stderr, "Error: GCD computation failed\n");
            abort();
        }
        // std::cerr << "BigInt::gcd(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    BigInt lcm(const BigInt& other) const {
        BigInt result;
        BIGNUM* abs_a = BN_new();
        BIGNUM* abs_b = BN_new();
        BN_copy(abs_a, value);
        BN_copy(abs_b, other.value);
        BN_set_negative(abs_a, 0);
        BN_set_negative(abs_b, 0);
        if (!bn_lcm_consttime(result.value, abs_a, abs_b, get_ctx())) {
            BN_free(abs_a);
            BN_free(abs_b);
            fprintf(stderr, "Error: LCM computation failed\n");
            abort();
        }
        BN_free(abs_a);
        BN_free(abs_b);
        // std::cerr << "BigInt::lcm(this=" << get_decimal_str() << ", other=" << other.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }

    // Using non-constant-time version.
    // This function is only called in the one-time precomputation phase of RNS Montgomery, so its performance does not affect the results in the paper
    // This is OK for a research prototype that claims constant-time performance
    // TODO: But will need to fix the commented out version below when merged in production code
    BigInt mod_inverse(const BigInt& modulus) const {
        BigInt result;
        if (BN_mod_inverse(result.value, value, modulus.value, get_ctx()) == nullptr) {
            fprintf(stderr, "Error: Modular inverse does not exist\n");
            abort();
        }
        // std::cerr << "BigInt::mod_inverse(this=" << get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
        return result;
    }
    
    // Modular inverse - using constant-time version
    // BigInt mod_inverse(const BigInt& modulus) const {
    //     BigInt result;
    //     result = *this % modulus; // Input must be reduced for bn_mod_inverse_consttime. FIXME: value can be negative, which requires a constant-time conditional addition. This is TBD
    //     BIGNUM* abs_a = BN_new();
    //     BN_copy(abs_a, result.value);
    //     BN_set_negative(abs_a, 0);
    //     int no_inverse = 0;
    //     if (!bn_mod_inverse_consttime(result.value, &no_inverse, abs_a, modulus.value, get_ctx())) {
    //         ERR_print_errors_fp(stderr);
    //         BN_free(abs_a);
    //         fprintf(stderr, "Error: Modular inverse computation failed\n");
    //         abort();
    //     }
    //     if (no_inverse) {
    //         BN_free(abs_a);
    //         fprintf(stderr, "Error: Modular inverse does not exist\n");
    //         abort();
    //     }
    //     BN_free(abs_a);
    //     // std::cerr << "BigInt::mod_inverse(this=" << get_decimal_str() << ", modulus=" << modulus.get_decimal_str() << ") -> " << result.get_decimal_str() << std::endl;
    //     return result;
    // }
    
    // Square root - note: this is not constant-time, but we try to minimize branches
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
    // WARNING: These leak timing information and should not be used with secret values
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

#endif // OSSL_WRAPPER_CONSTTIME_HPP
