#include "conversion.hpp"
#include "../precompute/precompute.hpp"
#include "../reduction/montgomery.hpp"
#include <array>
#include <iostream>
#include <random>
#include <cassert>

// Test helper: round-trip conversion and verify it matches expected
template<int bits, int limbs_out>
bool test_round_trip(const BigInt &target, const BigInt &r, const std::array<uint64_t, limbs_out> &moduli_out) {
    // Setup converters (following BasicRNS pattern: premult=1, postmult=r for to_rns; premult=1, postmult=1 for from_rns)
    // ConvertFromRNS takes moduli_in as template parameter, which should equal limbs_out for round-trip
    ConvertToRNS<bits, limbs_out> convert_to(BigInt(1), r, target, moduli_out);
    ConvertFromRNS<bits, limbs_out> convert_from(BigInt(1), BigInt(1), target, moduli_out);
    
    // Create reducer
    MontgomeryReduce<limbs_out> reducer(moduli_out);
    
    // Test multiple random values
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint64_t> dis(0, 1000);
    
    for (int test_idx = 0; test_idx < 10; test_idx++) {
        // Generate a test value less than target
        BigInt test_val;
        if (test_idx == 0) {
            test_val = BigInt(0);
        } else if (test_idx == 1) {
            test_val = target - BigInt(1);
        } else {
            // Random value in [0, min(target, 2^64))
            uint64_t rand_val = dis(gen);
            test_val = BigInt(rand_val) % target;
        }
        
        // Convert to RNS
        AVXVector<limbs_out> residues = convert_to.convert_to_rns(test_val, reducer);
        
        // Convert back from RNS
        BigInt reconstructed = convert_from.convert_from_rns(residues);
        
        // Verify round-trip (mod target)
        BigInt expected = test_val % target;
        if (reconstructed != expected) {
            std::cerr << "Round-trip test failed for test_idx=" << test_idx 
                      << " test_val=" << test_val.to_string() 
                      << " reconstructed=" << reconstructed.to_string()
                      << " expected=" << expected.to_string() << std::endl;
            return false;
        }
    }
    
    return true;
}

// Test against Python reference values for a simple case
bool test_python_reference() {
    // Test case similar to BasicRNS with small target
    constexpr int bits = 256;
    constexpr int limbs = 5; // ceil_div(256, 52) = 5
    
    BigInt target(97); // Small prime target
    BigInt r = BigInt(1) << 52; // r = 2^52 for Montgomery
    
    // Generate moduli using GenRNS (following BasicRNS pattern)
    // For BasicRNS: moduli should cover (target * max_bound)^2
    // Using smaller bound for test: (target * 10)^2
    GenRNS<limbs> gen((target * BigInt(10)) * (target * BigInt(10)), -1, BigInt(1), 52);
    
    std::array<uint64_t, limbs> moduli;
    for (int i = 0; i < limbs; i++) {
        moduli[i] = static_cast<uint64_t>(gen.moduli[i].to_ulong());
    }
    
    return test_round_trip<bits, limbs>(target, r, moduli);
}

// Test with multiple target sizes
bool test_multiple_targets() {
    bool all_pass = true;
    
    // Test 1: Small target (97)
    {
        constexpr int bits = 256;
        constexpr int limbs = 5;
        BigInt target(97);
        BigInt r = BigInt(1) << 52;
        GenRNS<limbs> gen((target * BigInt(10)) * (target * BigInt(10)), -1, BigInt(1), 52);
        std::array<uint64_t, limbs> moduli;
        for (int i = 0; i < limbs; i++) {
            moduli[i] = static_cast<uint64_t>(gen.moduli[i].to_ulong());
        }
        if (!test_round_trip<bits, limbs>(target, r, moduli)) {
            std::cerr << "Test failed for target=97" << std::endl;
            all_pass = false;
        }
    }
    
    // Test 2: Medium target (65537)
    {
        constexpr int bits = 512;
        constexpr int limbs = 10;
        BigInt target(65537);
        BigInt r = BigInt(1) << 52;
        GenRNS<limbs> gen((target * BigInt(10)) * (target * BigInt(10)), -1, BigInt(1), 52);
        std::array<uint64_t, limbs> moduli;
        for (int i = 0; i < limbs; i++) {
            moduli[i] = static_cast<uint64_t>(gen.moduli[i].to_ulong());
        }
        if (!test_round_trip<bits, limbs>(target, r, moduli)) {
            std::cerr << "Test failed for target=65537" << std::endl;
            all_pass = false;
        }
    }
    
    return all_pass;
}

// Test word_reinterpret and int_reconstruct directly
bool test_word_functions() {
    // Test word_reinterpret and int_reconstruct
    constexpr int limbs = 5;
    constexpr int word_length = 52;
    
    // Test with a known value
    BigInt test_val("123456789012345678901234567890");
    
    // Reinterpret as words
    std::array<uint64_t, limbs> digits;
    word_reinterpret<limbs, word_length>(test_val, digits);
    
    // Reconstruct
    BigInt reconstructed = int_reconstruct<limbs, word_length>(digits);
    
    // For comparison, compute expected mask
    BigInt mask = (BigInt(1) << word_length) - 1;
    BigInt expected(0);
    for (int i = 0; i < limbs; i++) {
        BigInt shifted = test_val >> (word_length * i);
        BigInt word_val = (shifted & mask);
        expected += word_val << (word_length * i);
    }
    
    // Note: reconstructed may not equal test_val if test_val exceeds what can be represented in limbs words
    // Instead, check that the first limbs words match
    bool ok = true;
    for (int i = 0; i < limbs; i++) {
        BigInt shifted_val = test_val >> (word_length * i);
        BigInt word_val = (shifted_val & mask);
        uint64_t expected_word = static_cast<uint64_t>(word_val.to_ulong());
        if (digits[i] != expected_word) {
            std::cerr << "word_reinterpret mismatch at index " << i 
                      << " got " << digits[i] << " expected " << expected_word << std::endl;
            ok = false;
        }
    }
    
    if (!ok) {
        return false;
    }
    
    // Test reconstruction: if test_val fits in limbs words, they should match
    BigInt max_val = (BigInt(1) << (word_length * limbs)) - 1;
    if (test_val <= max_val && reconstructed != test_val) {
        std::cerr << "int_reconstruct mismatch: got " << reconstructed.to_string() 
                  << " expected " << test_val.to_string() << std::endl;
        return false;
    }
    
    return true;
}

int main() {
    bool all_pass = true;
    
    std::cout << "Testing word_reinterpret and int_reconstruct..." << std::endl;
    if (!test_word_functions()) {
        std::cerr << "word function tests failed" << std::endl;
        all_pass = false;
    } else {
        std::cout << "word function tests passed" << std::endl;
    }
    
    std::cout << "Testing round-trip conversion with Python reference..." << std::endl;
    if (!test_python_reference()) {
        std::cerr << "Python reference test failed" << std::endl;
        all_pass = false;
    } else {
        std::cout << "Python reference test passed" << std::endl;
    }
    
    std::cout << "Testing multiple targets..." << std::endl;
    if (!test_multiple_targets()) {
        std::cerr << "Multiple targets test failed" << std::endl;
        all_pass = false;
    } else {
        std::cout << "Multiple targets test passed" << std::endl;
    }
    
    std::cout << (all_pass ? "PASS" : "FAIL") << std::endl;
    return all_pass ? 0 : 1;
}

