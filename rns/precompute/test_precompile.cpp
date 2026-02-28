#include "precompile.hpp"

// All tests use numbers > 2^64 to ensure multilimb logic is tested
// 2^64 = 18446744073709551616

// Test addition with multilimb numbers
static_assert(BigInt<128>("18446744073709551617") + BigInt<128>("18446744073709551618") == BigInt<128>("36893488147419103235"));
static_assert(BigInt<128>("123456789012345678901234567890") + BigInt<128>("987654321098765432109876543210") == BigInt<128>("1111111110111111111011111111100"));
static_assert(BigInt<128>("999999999999999999999999999999") + BigInt<128>("1") == BigInt<128>("1000000000000000000000000000000"));

// Test subtraction with multilimb numbers
static_assert(BigInt<128>("36893488147419103235") - BigInt<128>("18446744073709551618") == BigInt<128>("18446744073709551617"));
static_assert(BigInt<128>("1111111110111111111011111111100") - BigInt<128>("987654321098765432109876543210") == BigInt<128>("123456789012345678901234567890"));
static_assert(BigInt<128>("1000000000000000000000000000000") - BigInt<128>("1") == BigInt<128>("999999999999999999999999999999"));

// Test multiplication with multilimb numbers
static_assert(BigInt<128>("18446744073709551617") * BigInt<128>("2") == BigInt<128>("36893488147419103234"));
static_assert(BigInt<128>("12345678901234567890") * BigInt<128>("98765432109876543210") == BigInt<128>("1219326311370217952237463801111263526900"));
static_assert(BigInt<128>("100000000000000000000") * BigInt<128>("100000000000000000000") == BigInt<128>("10000000000000000000000000000000000000000"));

// Test division with multilimb numbers
static_assert(BigInt<128>("36893488147419103234") / BigInt<128>("18446744073709551617") == BigInt<128>("2"));
static_assert(BigInt<128>("12345678901234567890") / BigInt<128>("10") == BigInt<128>("1234567890123456789"));
static_assert(BigInt<128>("18446744073709551617") / BigInt<128>("2") == BigInt<128>("9223372036854775808"));

// Test modulo with multilimb numbers  
static_assert(BigInt<128>("36893488147419103235") % BigInt<128>("18446744073709551617") == BigInt<128>("1"));
static_assert(BigInt<128>("12345678901234567890") % BigInt<128>("10") == BigInt<128>("0"));
static_assert(BigInt<128>("18446744073709551617") % BigInt<128>("2") == BigInt<128>("1"));

// Test left shift with multilimb numbers
static_assert(BigInt<128>("18446744073709551617") << 1 == BigInt<128>("36893488147419103234"));
static_assert(BigInt<128>("18446744073709551617") << 64 == BigInt<128>("340282366920938463481821351505477763072"));
static_assert(BigInt<128>("12345678901234567890") << 10 == BigInt<128>("12641975194864197519360"));

// Test right shift with multilimb numbers
static_assert(BigInt<128>("36893488147419103234") >> 1 == BigInt<128>("18446744073709551617"));
// Test whole-limb shift: value that fits in 128 bits
static_assert(BigInt<128>("18446744073709551616") >> 64 == BigInt<128>("1"));
static_assert(BigInt<128>("12641975194864197519360") >> 10 == BigInt<128>("12345678901234567890"));

// Test bitwise AND with multilimb numbers
static_assert((BigInt<128>("18446744073709551617") & BigInt<128>("18446744073709551616")) == BigInt<128>("18446744073709551616"));
static_assert((BigInt<128>("18446744073709551619") & BigInt<128>("18446744073709551617")) == BigInt<128>("18446744073709551617"));
static_assert((BigInt<128>("340282366920938463481821351505477763071") & BigInt<128>("18446744073709551615")) == BigInt<128>("18446744073709551615"));

// Test comparison operators with multilimb numbers
static_assert(BigInt<128>("18446744073709551617") < BigInt<128>("18446744073709551618"));
static_assert(BigInt<128>("18446744073709551618") > BigInt<128>("18446744073709551617"));
static_assert(BigInt<128>("18446744073709551617") <= BigInt<128>("18446744073709551618"));
static_assert(BigInt<128>("18446744073709551618") >= BigInt<128>("18446744073709551617"));
static_assert(BigInt<128>("18446744073709551617") == BigInt<128>("18446744073709551617"));
static_assert(BigInt<128>("18446744073709551617") != BigInt<128>("18446744073709551618"));

int main() {
    return 0;
}