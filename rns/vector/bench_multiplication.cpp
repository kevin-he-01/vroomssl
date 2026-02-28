
// Benchmark multiplication in vector module using mulreduce (batch size 1).
// Uses the same parameters as bench_batching but calls mul_reduce directly.

#include "multiplication.hpp"
#include "../precompute/gmp_wrapper.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include "../../scripts/test_values.hpp"

// ceil_log2 is not defined in vector headers, so define it here
// ceil_div is already defined in avx512ifma.hpp/fallback.hpp via vector_impl.hpp
constexpr int ceil_log2(int x) {
    return (x == 0) ? 0 : 32 - __builtin_clz(x);
}

template<int modulus_bits>
void benchmark_multiplication(const BigInt& q) {
    const int iterations = 10000;
    constexpr int num_additions = 36;  // Same as bench_batching uses Ring<modulus_bits, 36>
    constexpr int max_bound = (3 + num_additions) * (3 + num_additions);
    constexpr int element_modbits = 52;
    constexpr int limbs = ceil_div(modulus_bits + ceil_log2(max_bound), element_modbits);
    
    IntRNS2System<modulus_bits, limbs> sys = make_IntRNS2SystemMontgomery<modulus_bits, limbs>(q, element_modbits, max_bound);
    
    BigInt a = BigInt::random(q);
    BigInt b = BigInt::random(q);
    
    auto [a_m1, a_m2] = sys.to_mont_avx(a);
    auto [b_m1, b_m2] = sys.to_mont_avx(b);
    
    AVXVector<limbs> out_m1, out_m2;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        sys.mul_reduce(a_m1, a_m2, b_m1, b_m2, out_m1, out_m2);
        // Update a for next iteration (a = a * b)
        a_m1 = out_m1;
        a_m2 = out_m2;
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    BigInt result = sys.from_mont_avx(out_m2);
    BigInt expected = (a * b.pow(iterations, q)) % q;
    if (result != expected) {
        std::cerr << "Multiplication result mismatch" << std::endl;
        std::cerr << "  Expected: " << expected.to_string() << std::endl;
        std::cerr << "  Got: " << result.to_string() << std::endl;
        return;
    }
    
    std::chrono::duration<double, std::micro> duration = end - start;
    auto time_per_op = duration.count() / iterations;
    std::cout << "Modulus bits: " << modulus_bits << std::endl;
    std::cout << "Time per operation: " << time_per_op << " microseconds" << std::endl;
}

int main() {
    // Original 381-bit modulus
    BigInt q = BigInt(modulus);
    std::cout << "Modulus: " << q.to_string() << std::endl;
    benchmark_multiplication<381>(q);
    
    // 2048-bit modulus from hex, random number found on the internet: https://stackoverflow.com/questions/22079315/i-need-2048bit-primes-in-order-to-test-the-upper-limits-of-my-rsa-program
    const char* modulus_2048_hex = "ceb052c9732614fee3c0a197a5ae0fcd83422243918ab83bc678656ae0344232a7c1070b7d5aabaae2bda96bf590da4830238b606f24b29626f1bfa00cce39f5f9bb9c1c3ead98f2055e373abf01e1fe1c816e12e0ed13791461c435123dad8cbe80e474f753aa9d115a8b93c167adceaee5a18ceedef88d307427fc495d9e44d4268ba83c4a65c4667b7df79f342639da3ddd2777926848855ca0068668efe7f27d65f455074c960bbc168bfb3a1225cd6f42585ddba6b3484f36707524133b81dd01d062591fec1b756766aeebe667bf9e2480eebb5964bc5eaff4b165e142772ce64b229a7258667a3964f08e06dfbfe3e3c1cf918395b89c1fdb18907711";
    BigInt q2048 = BigInt(modulus_2048_hex, 16);
    size_t bit_length = q2048.bit_length();
    std::cout << "\n2048-bit Modulus (hex): " << modulus_2048_hex << std::endl;
    std::cout << "Modulus bit length: " << bit_length << " bits" << std::endl;
    if (bit_length != 2048) {
        std::cerr << "Warning: Expected 2048 bits, got " << bit_length << " bits" << std::endl;
    }
    std::cout << "Modulus (decimal): " << q2048.to_string() << std::endl;
    std::cout << "\nBenchmarking 2048-bit modulus:\n" << std::endl;
    benchmark_multiplication<2048>(q2048);
}

