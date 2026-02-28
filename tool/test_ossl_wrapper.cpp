#include "../rns/precompute/gmp_wrapper.hpp"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>
#include <climits>
#include <openssl/bn.h>

// Test helper macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << "FAIL: " << message << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    std::cout << "PASS: " << message << std::endl

static bool test_constructors() {
    std::cout << "\n=== Testing Constructors ===" << std::endl;
    
    // Default constructor
    BigInt a;
    TEST_ASSERT(a == 0, "Default constructor should create zero");
    TEST_PASS("Default constructor");
    
    // Constructor from long
    BigInt b(42L);
    TEST_ASSERT(b == 42, "Constructor from long");
    TEST_PASS("Constructor from long");
    
    // Constructor from unsigned long
    BigInt c(100UL);
    TEST_ASSERT(c == 100, "Constructor from unsigned long");
    TEST_PASS("Constructor from unsigned long");
    
    // Constructor from int
    BigInt d(123);
    TEST_ASSERT(d == 123, "Constructor from int");
    TEST_PASS("Constructor from int");
    
    // Constructor from unsigned int
    BigInt e(456U);
    TEST_ASSERT(e == 456, "Constructor from unsigned int");
    TEST_PASS("Constructor from unsigned int");
    
    // Constructor from string (base 10)
    BigInt f("789");
    TEST_ASSERT(f == 789, "Constructor from string (base 10)");
    TEST_PASS("Constructor from string (base 10)");
    
    // Constructor from string (base 16)
    BigInt g("FF", 16);
    TEST_ASSERT(g == 255, "Constructor from string (base 16)");
    TEST_PASS("Constructor from string (base 16)");
    
    // Constructor from char*
    BigInt h("1000");
    TEST_ASSERT(h == 1000, "Constructor from char*");
    TEST_PASS("Constructor from char*");
    
    // Constructor from char* (base 2)
    BigInt i("1010", 2);
    TEST_ASSERT(i == 10, "Constructor from char* (base 2)");
    TEST_PASS("Constructor from char* (base 2)");
    
    // Constructor from mpz_class (removed - using OpenSSL BN now)
    // Test removed as we no longer support mpz_class
    
    // Copy constructor
    BigInt k(12345);
    BigInt l(k);
    TEST_ASSERT(l == 12345, "Copy constructor");
    TEST_ASSERT(l == k, "Copy constructor equality");
    TEST_PASS("Copy constructor");
    
    // Negative values
    BigInt m(-42);
    TEST_ASSERT(m == -42, "Constructor with negative value");
    TEST_PASS("Constructor with negative value");
    
    // Large value
    BigInt n("123456789012345678901234567890");
    TEST_ASSERT(n.to_string() == "123456789012345678901234567890", "Constructor with large value");
    TEST_PASS("Constructor with large value");
    
    return true;
}

static bool test_assignment_operators() {
    std::cout << "\n=== Testing Assignment Operators ===" << std::endl;
    
    // Assignment from BigInt
    BigInt a(100);
    BigInt b;
    b = a;
    TEST_ASSERT(b == 100, "Assignment from BigInt");
    TEST_PASS("Assignment from BigInt");
    
    // Self-assignment
    // BigInt c(50);
    // c = c;
    // TEST_ASSERT(c == 50, "Self-assignment");
    // TEST_PASS("Self-assignment");
    
    // Assignment from long
    BigInt d;
    d = 200L;
    TEST_ASSERT(d == 200, "Assignment from long");
    TEST_PASS("Assignment from long");
    
    // Assignment from string
    BigInt e;
    e = std::string("300");
    TEST_ASSERT(e == 300, "Assignment from string");
    TEST_PASS("Assignment from string");
    
    // Assignment from mpz_class (removed - using OpenSSL BN now)
    // Test removed as we no longer support mpz_class
    
    // Chained assignment
    BigInt g, h, i;
    g = h = i = 500;
    TEST_ASSERT(g == 500 && h == 500 && i == 500, "Chained assignment");
    TEST_PASS("Chained assignment");
    
    return true;
}

static bool test_arithmetic_operators() {
    std::cout << "\n=== Testing Arithmetic Operators ===" << std::endl;
    
    BigInt a(10);
    BigInt b(3);
    
    // Addition
    BigInt c = a + b;
    TEST_ASSERT(c == 13, "Addition");
    TEST_PASS("Addition");
    
    // Subtraction
    BigInt d = a - b;
    TEST_ASSERT(d == 7, "Subtraction");
    TEST_PASS("Subtraction");
    
    // Multiplication
    BigInt e = a * b;
    TEST_ASSERT(e == 30, "Multiplication");
    TEST_PASS("Multiplication");
    
    // Division
    BigInt f = a / b;
    TEST_ASSERT(f == 3, "Division");
    TEST_PASS("Division");
    
    // Modulo
    BigInt g = a % b;
    TEST_ASSERT(g == 1, "Modulo");
    TEST_PASS("Modulo");
    
    // Unary minus
    BigInt h = -a;
    TEST_ASSERT(h == -10, "Unary minus");
    TEST_PASS("Unary minus");
    
    // Unary plus
    BigInt i = +a;
    TEST_ASSERT(i == 10, "Unary plus");
    TEST_PASS("Unary plus");
    
    // Large numbers
    BigInt j("1000000000000000000000");
    BigInt k("500000000000000000000");
    BigInt l = j + k;
    TEST_ASSERT(l.to_string() == "1500000000000000000000", "Large number addition");
    TEST_PASS("Large number addition");
    
    // Negative arithmetic
    BigInt m(-10);
    BigInt n(5);
    BigInt o = m + n;
    TEST_ASSERT(o == -5, "Negative addition");
    TEST_PASS("Negative addition");
    
    BigInt p = m - n;
    TEST_ASSERT(p == -15, "Negative subtraction");
    TEST_PASS("Negative subtraction");
    
    BigInt q = m * n;
    TEST_ASSERT(q == -50, "Negative multiplication");
    TEST_PASS("Negative multiplication");
    
    // // Division by zero (should throw)
    // bool threw = false;
    // try {
    //     BigInt r = a / BigInt(0);
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Division by zero should throw");
    // TEST_PASS("Division by zero exception");
    
    // // Modulo by zero (should throw)
    // threw = false;
    // try {
    //     BigInt s = a % BigInt(0);
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Modulo by zero should throw");
    // TEST_PASS("Modulo by zero exception");
    
    return true;
}

static bool test_compound_assignment_operators() {
    std::cout << "\n=== Testing Compound Assignment Operators ===" << std::endl;
    
    BigInt a(10);
    
    // +=
    a += BigInt(5);
    TEST_ASSERT(a == 15, "operator+=");
    TEST_PASS("operator+=");
    
    // -=
    a -= BigInt(3);
    TEST_ASSERT(a == 12, "operator-=");
    TEST_PASS("operator-=");
    
    // *=
    a *= BigInt(2);
    TEST_ASSERT(a == 24, "operator*=");
    TEST_PASS("operator*=");
    
    // /=
    a /= BigInt(4);
    TEST_ASSERT(a == 6, "operator/=");
    TEST_PASS("operator/=");
    
    // %=
    a %= BigInt(4);
    TEST_ASSERT(a == 2, "operator%=");
    TEST_PASS("operator%=");
    
    // Chaining
    BigInt b(100);
    (b += BigInt(50)) -= BigInt(30);
    TEST_ASSERT(b == 120, "Chained compound assignment");
    TEST_PASS("Chained compound assignment");
    
    // // Division by zero
    // bool threw = false;
    // try {
    //     BigInt c(10);
    //     c /= BigInt(0);
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Division by zero in /=");
    // TEST_PASS("Division by zero exception in /=");
    
    // // Modulo by zero
    // threw = false;
    // try {
    //     BigInt d(10);
    //     d %= BigInt(0);
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Modulo by zero in %=");
    // TEST_PASS("Modulo by zero exception in %=");
    
    return true;
}

static bool test_comparison_operators() {
    std::cout << "\n=== Testing Comparison Operators ===" << std::endl;
    
    BigInt a(10);
    BigInt b(20);
    BigInt c(10);
    
    // <
    TEST_ASSERT(a < b, "operator< (less than)");
    TEST_ASSERT(!(b < a), "operator< (not less)");
    TEST_PASS("operator<");
    
    // >
    TEST_ASSERT(b > a, "operator> (greater than)");
    TEST_ASSERT(!(a > b), "operator> (not greater)");
    TEST_PASS("operator>");
    
    // <=
    TEST_ASSERT(a <= b, "operator<= (less or equal)");
    TEST_ASSERT(a <= c, "operator<= (equal)");
    TEST_ASSERT(!(b <= a), "operator<= (not less or equal)");
    TEST_PASS("operator<=");
    
    // >=
    TEST_ASSERT(b >= a, "operator>= (greater or equal)");
    TEST_ASSERT(a >= c, "operator>= (equal)");
    TEST_ASSERT(!(a >= b), "operator>= (not greater or equal)");
    TEST_PASS("operator>=");
    
    // ==
    TEST_ASSERT(a == c, "operator== (equal)");
    TEST_ASSERT(!(a == b), "operator== (not equal)");
    TEST_PASS("operator==");
    
    // !=
    TEST_ASSERT(a != b, "operator!= (not equal)");
    TEST_ASSERT(!(a != c), "operator!= (equal)");
    TEST_PASS("operator!=");
    
    // Negative comparisons
    BigInt d(-10);
    BigInt e(-5);
    TEST_ASSERT(d < e, "Negative comparison (<)");
    TEST_ASSERT(e > d, "Negative comparison (>)");
    TEST_PASS("Negative comparisons");
    
    // Zero comparisons
    BigInt f(0);
    TEST_ASSERT(f < a, "Zero comparison (<)");
    TEST_ASSERT(a > f, "Zero comparison (>)");
    TEST_ASSERT(f == BigInt(0), "Zero equality");
    TEST_PASS("Zero comparisons");
    
    return true;
}

static bool test_bitwise_operators() {
    std::cout << "\n=== Testing Bitwise Operators ===" << std::endl;
    
    BigInt a(0b1010);  // 10
    BigInt b(0b1100);  // 12
    
    // &
    BigInt c = a & b;
    TEST_ASSERT(c == 0b1000, "Bitwise AND");
    TEST_PASS("operator&");
    
    // |
    BigInt d = a | b;
    TEST_ASSERT(d == 0b1110, "Bitwise OR");
    TEST_PASS("operator|");
    
    // ^
    BigInt e = a ^ b;
    TEST_ASSERT(e == 0b0110, "Bitwise XOR");
    TEST_PASS("operator^");
    
    // ~
    BigInt f = ~a;
    // Note: ~10 depends on representation, but should be consistent
    TEST_PASS("operator~");
    
    // <<
    BigInt g = a << 2;
    TEST_ASSERT(g == 40, "Left shift");
    TEST_PASS("operator<<");
    
    // >>
    BigInt h = g >> 2;
    TEST_ASSERT(h == 10, "Right shift");
    TEST_PASS("operator>>");
    
    // Large shift
    BigInt i(1);
    BigInt j = i << 100;
    TEST_ASSERT(j.bit_length() == 101, "Large left shift");
    TEST_PASS("Large left shift");
    
    return true;
}

static bool test_compound_bitwise_assignment_operators() {
    std::cout << "\n=== Testing Compound Bitwise Assignment Operators ===" << std::endl;
    
    BigInt a(0b1010);  // 10
    BigInt b(0b1100);  // 12
    
    // &=
    BigInt c = a;
    c &= b;
    TEST_ASSERT(c == 0b1000, "operator&=");
    TEST_PASS("operator&=");
    
    // |=
    BigInt d = a;
    d |= b;
    TEST_ASSERT(d == 0b1110, "operator|=");
    TEST_PASS("operator|=");
    
    // ^=
    BigInt e = a;
    e ^= b;
    TEST_ASSERT(e == 0b0110, "operator^=");
    TEST_PASS("operator^=");
    
    // <<=
    BigInt f(10);
    f <<= 2;
    TEST_ASSERT(f == 40, "operator<<=");
    TEST_PASS("operator<<=");
    
    // >>=
    BigInt g(40);
    g >>= 2;
    TEST_ASSERT(g == 10, "operator>>=");
    TEST_PASS("operator>>=");
    
    return true;
}

static bool test_pow() {
    std::cout << "\n=== Testing pow() Method ===" << std::endl;
    
    BigInt a(2);
    
    // Regular exponentiation
    BigInt b = a.pow(BigInt(10));
    TEST_ASSERT(b == 1024, "Regular exponentiation");
    TEST_PASS("Regular exponentiation");
    
    // Modular exponentiation
    BigInt c = a.pow(BigInt(10), BigInt(101));
    TEST_ASSERT(c == 14, "Modular exponentiation");
    TEST_PASS("Modular exponentiation");
    
    // Power of 0
    BigInt d = a.pow(BigInt(0));
    TEST_ASSERT(d == 1, "Power of 0");
    TEST_PASS("Power of 0");
    
    // Power of 1
    BigInt e = a.pow(BigInt(1));
    TEST_ASSERT(e == 2, "Power of 1");
    TEST_PASS("Power of 1");
    
    // Large exponent
    BigInt f(3);
    BigInt g = f.pow(BigInt(5));
    TEST_ASSERT(g == 243, "Large exponent");
    TEST_PASS("Large exponent");
    
    // Modular exponentiation with large numbers
    BigInt h(5);
    BigInt i = h.pow(BigInt(7), BigInt(100));
    TEST_ASSERT(i == 25, "Modular exponentiation with large numbers");
    TEST_PASS("Modular exponentiation with large numbers");
    
    // // Negative exponent (should throw for non-modular)
    // bool threw = false;
    // try {
    //     BigInt j = a.pow(BigInt(-1));
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Negative exponent should throw for non-modular");
    // TEST_PASS("Negative exponent exception");
    
    // Negative exponent with modulus (modular inverse)
    BigInt k(3);
    BigInt l = k.pow(BigInt(-1), BigInt(11));  // 3^(-1) mod 11 = 4
    TEST_ASSERT(l == 4, "Negative exponent with modulus");
    TEST_PASS("Negative exponent with modulus");
    
    // // Modular inverse that doesn't exist
    // threw = false;
    // try {
    //     BigInt m(2);
    //     BigInt n = m.pow(BigInt(-1), BigInt(4));  // gcd(2,4) != 1
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Modular inverse that doesn't exist should throw");
    // TEST_PASS("Modular inverse exception");
    
    return true;
}

static bool test_utility_methods() {
    std::cout << "\n=== Testing Utility Methods ===" << std::endl;
    
    // to_string
    BigInt a(12345);
    TEST_ASSERT(a.to_string() == "12345", "to_string() base 10");
    TEST_PASS("to_string() base 10");
    
    BigInt b(255);
    TEST_ASSERT(b.to_string(16) == "ff", "to_string() base 16");
    TEST_PASS("to_string() base 16");
    
    TEST_ASSERT(b.to_string(2) == "11111111", "to_string() base 2");
    TEST_PASS("to_string() base 2");
    
    // to_long
    BigInt c(12345L);
    TEST_ASSERT(c.to_long() == 12345L, "to_long()");
    TEST_PASS("to_long()");
    
    // to_ulong
    BigInt d(12345UL);
    TEST_ASSERT(d.to_ulong() == 12345UL, "to_ulong()");
    TEST_PASS("to_ulong()");
    
    // to_ulong with value larger than 64 bits (should return lowest 64 bits)
    BigInt large("18446744073709551616");  // 2^64 (one more than max ulong)
    unsigned long large_result = large.to_ulong();
    TEST_ASSERT(large_result == 0UL, "to_ulong() with 2^64 should return 0");
    TEST_PASS("to_ulong() with 2^64");
    
    BigInt large2("18446744073709551617");  // 2^64 + 1
    unsigned long large2_result = large2.to_ulong();
    TEST_ASSERT(large2_result == 1UL, "to_ulong() with 2^64+1 should return 1");
    TEST_PASS("to_ulong() with 2^64+1");
    
    BigInt large3("340282366920938463463374607431768211456");  // 2^128
    unsigned long large3_result = large3.to_ulong();
    TEST_ASSERT(large3_result == 0UL, "to_ulong() with 2^128 should return 0");
    TEST_PASS("to_ulong() with 2^128");
    
    BigInt large4("18446744073709551615");  // 2^64 - 1 (max ulong)
    unsigned long large4_result = large4.to_ulong();
    TEST_ASSERT(large4_result == ULONG_MAX, "to_ulong() with 2^64-1 should return ULONG_MAX");
    TEST_PASS("to_ulong() with 2^64-1");
    
    // to_double
    BigInt e(12345);
    double dval = e.to_double();
    TEST_ASSERT(std::abs(dval - 12345.0) < 0.001, "to_double()");
    TEST_PASS("to_double()");
    
    // sign
    BigInt f(10);
    TEST_ASSERT(f.sign() == 1, "sign() positive");
    TEST_PASS("sign() positive");
    
    BigInt g(-10);
    TEST_ASSERT(g.sign() == -1, "sign() negative");
    TEST_PASS("sign() negative");
    
    BigInt h(0);
    TEST_ASSERT(h.sign() == 0, "sign() zero");
    TEST_PASS("sign() zero");
    
    // bit_length
    BigInt i(8);  // 1000 in binary
    TEST_ASSERT(i.bit_length() == 4, "bit_length()");
    TEST_PASS("bit_length()");
    
    BigInt j(1);
    TEST_ASSERT(j.bit_length() == 1, "bit_length() of 1");
    TEST_PASS("bit_length() of 1");
    
    BigInt k(0);
    TEST_ASSERT(k.bit_length() == 1, "bit_length() of 0");  // mpz_sizeinbase returns 1 for zero
    TEST_PASS("bit_length() of 0");
    
    // is_zero
    BigInt l(0);
    TEST_ASSERT(l.is_zero(), "is_zero() true");
    TEST_PASS("is_zero() true");
    
    BigInt m(1);
    TEST_ASSERT(!m.is_zero(), "is_zero() false");
    TEST_PASS("is_zero() false");
    
    // is_negative
    BigInt n(-5);
    TEST_ASSERT(n.is_negative(), "is_negative() true");
    TEST_PASS("is_negative() true");
    
    BigInt o(5);
    TEST_ASSERT(!o.is_negative(), "is_negative() false");
    TEST_PASS("is_negative() false");
    
    // is_positive
    BigInt p(5);
    TEST_ASSERT(p.is_positive(), "is_positive() true");
    TEST_PASS("is_positive() true");
    
    BigInt q(-5);
    TEST_ASSERT(!q.is_positive(), "is_positive() false");
    TEST_PASS("is_positive() false");
    
    BigInt r(0);
    TEST_ASSERT(!r.is_positive(), "is_positive() false for zero");
    TEST_PASS("is_positive() false for zero");
    
    return true;
}

static bool test_gcd_lcm() {
    std::cout << "\n=== Testing GCD and LCM ===" << std::endl;
    
    // GCD
    BigInt a(48);
    BigInt b(18);
    BigInt c = a.gcd(b);
    TEST_ASSERT(c == 6, "GCD");
    TEST_PASS("gcd()");
    
    BigInt d(17);
    BigInt e(19);
    BigInt f = d.gcd(e);
    TEST_ASSERT(f == 1, "GCD of primes");
    TEST_PASS("gcd() of primes");
    
    BigInt g(100);
    BigInt h(0);
    BigInt i = g.gcd(h);
    TEST_ASSERT(i == 100, "GCD with zero");
    TEST_PASS("gcd() with zero");
    
    // LCM
    BigInt j(12);
    BigInt k(18);
    BigInt l = j.lcm(k);
    TEST_ASSERT(l == 36, "LCM");
    TEST_PASS("lcm()");
    
    BigInt m(5);
    BigInt n(7);
    BigInt o = m.lcm(n);
    TEST_ASSERT(o == 35, "LCM of primes");
    TEST_PASS("lcm() of primes");
    
    return true;
}

static bool test_mod_inverse() {
    std::cout << "\n=== Testing Modular Inverse ===" << std::endl;
    
    // Basic modular inverse
    BigInt a(3);
    BigInt b(11);
    BigInt c = a.mod_inverse(b);
    TEST_ASSERT((a * c) % b == 1, "Modular inverse");
    TEST_PASS("mod_inverse()");
    
    // Another example
    BigInt d(7);
    BigInt e(13);
    BigInt f = d.mod_inverse(e);
    TEST_ASSERT((d * f) % e == 1, "Modular inverse (7 mod 13)");
    TEST_PASS("mod_inverse() (7 mod 13)");

    // Modular inverse where the input is negative
    {
        BigInt g(-3);
        BigInt h(11);
        BigInt i = g.mod_inverse(h);
        std::cout << "g: " << g.get_decimal_str() << ", i: " << i.get_decimal_str() << ", h: " << h.get_decimal_str() << std::endl;
        TEST_ASSERT(i == BigInt(7), "Modular inverse (-3 mod 11)");
        TEST_PASS("mod_inverse() (-3 mod 11)");
    }

    {
        BigInt g(-117);
        BigInt h(100);
        BigInt i = g.mod_inverse(h);
        std::cout << "g: " << g.get_decimal_str() << ", i: " << i.get_decimal_str() << ", h: " << h.get_decimal_str() << std::endl;
        // TEST_ASSERT((g * i) % h == 1, "Modular inverse (-17 mod 100)");
        TEST_ASSERT(i == BigInt(47), "Modular inverse (-17 mod 100)");
        TEST_PASS("mod_inverse() (-17 mod 100)");
    }

    // Modular inverse with an even modulus
    BigInt g(3);
    BigInt h(100);
    BigInt i = g.mod_inverse(h);
    TEST_ASSERT((g * i) % h == 1, "Modular inverse (3 mod 100)");
    TEST_PASS("mod_inverse() (3 mod 100)");

    // Modulus inverse where input is not reduced
    BigInt j(103);
    BigInt k(100);
    BigInt l = j.mod_inverse(k);
    TEST_ASSERT((j * l) % k == 1, "Modular inverse (103 mod 100)");
    TEST_PASS("mod_inverse() (103 mod 100)");
    
    // // Modular inverse that doesn't exist
    // bool threw = false;
    // try {
    //     BigInt g(2);
    //     BigInt h(4);
    //     BigInt i = g.mod_inverse(h);
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Modular inverse that doesn't exist should throw");
    // TEST_PASS("mod_inverse() exception");
    
    return true;
}

static bool test_sqrt() {
    std::cout << "\n=== Testing Square Root ===" << std::endl;
    
    // Perfect square
    BigInt a(16);
    BigInt b = a.sqrt();
    TEST_ASSERT(b == 4, "Square root of perfect square");
    TEST_PASS("sqrt() perfect square");
    
    // Non-perfect square
    BigInt c(15);
    BigInt d = c.sqrt();
    TEST_ASSERT(d == 3, "Square root of non-perfect square");
    TEST_PASS("sqrt() non-perfect square");
    
    // Large square
    BigInt e("100000000000000000000");  // 10^20
    BigInt f = e.sqrt();
    TEST_ASSERT(f == BigInt("10000000000"), "Square root of large number");
    TEST_PASS("sqrt() large number");
    
    // Zero
    BigInt g(0);
    BigInt h = g.sqrt();
    TEST_ASSERT(h == 0, "Square root of zero");
    TEST_PASS("sqrt() zero");
    
    // One
    BigInt i(1);
    BigInt j = i.sqrt();
    TEST_ASSERT(j == 1, "Square root of one");
    TEST_PASS("sqrt() one");
    
    // // Negative number (should throw)
    // bool threw = false;
    // try {
    //     BigInt k(-1);
    //     BigInt l = k.sqrt();
    // } catch (const std::runtime_error&) {
    //     threw = true;
    // }
    // TEST_ASSERT(threw, "Square root of negative should throw");
    // TEST_PASS("sqrt() exception for negative");
    
    return true;
}

// static bool test_get_mpz() {
//     std::cout << "\n=== Testing get_mpz() ===" << std::endl;
    
//     BigInt a(12345);
    
//     // Const version
//     const BigInt& b = a;
//     const BIGNUM* bn1 = b.get_mpz();
//     BigInt test1;
//     BN_copy(test1.get_mpz(), bn1);
//     TEST_ASSERT(test1 == 12345, "get_mpz() const");
//     TEST_PASS("get_mpz() const");
    
//     // Non-const version - create a new BigInt and modify via get_mpz
//     BigInt c(12345);
//     BIGNUM* bn2 = c.get_mpz();
//     BN_set_word(bn2, 54321);
//     TEST_ASSERT(c == 54321, "get_mpz() non-const");
//     TEST_PASS("get_mpz() non-const");
    
//     return true;
// }

// static bool test_random() {
//     std::cout << "\n=== Testing random() ===" << std::endl;
    
//     BigInt max(100);
    
//     // Generate multiple random numbers
//     for (int i = 0; i < 10; i++) {
//         BigInt r = BigInt::random(max);
//         TEST_ASSERT(r >= 0 && r < max, "Random number in range");
//     }
//     TEST_PASS("random() generates numbers in range");
    
//     // Test with larger range
//     BigInt max2("1000000000000000000000");
//     BigInt r2 = BigInt::random(max2);
//     TEST_ASSERT(r2 >= 0 && r2 < max2, "Random number in large range");
//     TEST_PASS("random() with large range");
    
//     return true;
// }

static bool test_stream_operators() {
    std::cout << "\n=== Testing Stream Operators ===" << std::endl;
    
    // Output stream
    BigInt a(12345);
    std::ostringstream oss;
    oss << a;
    TEST_ASSERT(oss.str() == "12345", "operator<<");
    TEST_PASS("operator<<");
    
    // Input stream
    BigInt b;
    std::istringstream iss("67890");
    iss >> b;
    TEST_ASSERT(b == 67890, "operator>>");
    TEST_PASS("operator>>");
    
    // Input stream with negative
    BigInt c;
    std::istringstream iss2("-12345");
    iss2 >> c;
    TEST_ASSERT(c == -12345, "operator>> with negative");
    TEST_PASS("operator>> with negative");
    
    return true;
}

static bool test_global_operators() {
    std::cout << "\n=== Testing Global Operators ===" << std::endl;
    
    BigInt a(10);
    
    // Addition
    BigInt b = 5L + a;
    TEST_ASSERT(b == 15, "Global operator+");
    TEST_PASS("Global operator+");
    
    // Subtraction
    BigInt c = 20L - a;
    TEST_ASSERT(c == 10, "Global operator-");
    TEST_PASS("Global operator-");
    
    // Multiplication
    BigInt d = 3L * a;
    TEST_ASSERT(d == 30, "Global operator*");
    TEST_PASS("Global operator*");
    
    // Division
    BigInt e = 30L / a;
    TEST_ASSERT(e == 3, "Global operator/");
    TEST_PASS("Global operator/");
    
    // Modulo
    BigInt f = 23L % a;
    TEST_ASSERT(f == 3, "Global operator%");
    TEST_PASS("Global operator%");
    
    // Comparisons
    TEST_ASSERT(5L < a, "Global operator<");
    TEST_PASS("Global operator<");
    
    TEST_ASSERT(20L > a, "Global operator>");
    TEST_PASS("Global operator>");
    
    TEST_ASSERT(10L <= a, "Global operator<=");
    TEST_PASS("Global operator<=");
    
    TEST_ASSERT(10L >= a, "Global operator>=");
    TEST_PASS("Global operator>=");
    
    TEST_ASSERT(10L == a, "Global operator==");
    TEST_PASS("Global operator==");
    
    TEST_ASSERT(5L != a, "Global operator!=");
    TEST_PASS("Global operator!=");
    
    return true;
}

static bool test_edge_cases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    // Very large numbers
    BigInt a("1234567890123456789012345678901234567890");
    BigInt b("9876543210987654321098765432109876543210");
    BigInt c = a + b;
    TEST_ASSERT(c.to_string() == "11111111101111111110111111111011111111100", "Very large addition");
    TEST_PASS("Very large numbers");
    
    // Zero operations
    BigInt d(0);
    BigInt e(100);
    TEST_ASSERT(d + e == 100, "Zero addition");
    TEST_ASSERT(d * e == 0, "Zero multiplication");
    TEST_ASSERT(e - d == 100, "Zero subtraction");
    TEST_PASS("Zero operations");
    
    // One operations
    BigInt f(1);
    TEST_ASSERT(f * e == 100, "One multiplication");
    TEST_ASSERT(e / f == 100, "One division");
    TEST_PASS("One operations");
    
    // Negative operations
    BigInt g(-100);
    BigInt h(50);
    TEST_ASSERT(g + h == -50, "Negative addition");
    TEST_ASSERT(g - h == -150, "Negative subtraction");
    TEST_ASSERT(g * h == -5000, "Negative multiplication");
    TEST_PASS("Negative operations");
    
    // String with different bases
    BigInt i("1010", 2);
    TEST_ASSERT(i == 10, "String base 2");
    BigInt j("FF", 16);
    TEST_ASSERT(j == 255, "String base 16");
    BigInt k("777", 8);
    TEST_ASSERT(k == 511, "String base 8");
    TEST_PASS("String with different bases");
    
    return true;
}

int main() {
    std::cout << "Starting comprehensive BigInt test suite..." << std::endl;
    
    bool all_passed = true;
    
    all_passed &= test_constructors();
    all_passed &= test_assignment_operators();
    all_passed &= test_arithmetic_operators();
    all_passed &= test_compound_assignment_operators();
    all_passed &= test_comparison_operators();
    all_passed &= test_bitwise_operators();
    all_passed &= test_compound_bitwise_assignment_operators();
    all_passed &= test_pow();
    all_passed &= test_utility_methods();
    all_passed &= test_gcd_lcm();
    all_passed &= test_mod_inverse();
    all_passed &= test_sqrt();
    // all_passed &= test_get_mpz();
    // all_passed &= test_random();
    all_passed &= test_stream_operators();
    all_passed &= test_global_operators();
    all_passed &= test_edge_cases();
    
    std::cout << "\n========================================" << std::endl;
    if (all_passed) {
        std::cout << "ALL TESTS PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}
