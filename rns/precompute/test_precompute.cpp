#include "precompute.hpp"
#include <array>
#include <iostream>

int main() {
    constexpr size_t LIMBS_IN = 2;
    constexpr size_t LIMBS_OUT = 2;
    constexpr size_t INDIGITS = 1;
    constexpr int INBITS = 8;
    constexpr size_t OUTDIGITS = 1;
    constexpr int OUTBITS = 8;
    constexpr int PRECISION = 16;

    std::array<uint64_t, LIMBS_IN> moduli_in{17, 13};
    std::array<uint64_t, LIMBS_OUT> moduli_out{17, 13};
    BigInt target_modulus(0); // use default = modulus_out
    BigInt premult(1);
    BigInt postmult(1);

    std::array<std::array<uint64_t, LIMBS_OUT * OUTDIGITS>, LIMBS_IN * INDIGITS> rns_values_tf{};
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> correction{};
    std::array<BigInt, LIMBS_IN * INDIGITS> sqe_tf{};

    gen_precompute_wide_min<LIMBS_IN, LIMBS_OUT, INDIGITS, INBITS, OUTDIGITS, OUTBITS, PRECISION>(
        moduli_in, moduli_out, target_modulus, premult, postmult, rns_values_tf, correction, sqe_tf);

    // Expected from Python gen_precompute_wide_min([17,13], None, [17,13], 1,1,1,8,1,8,16)
    std::array<std::array<uint64_t, LIMBS_OUT * OUTDIGITS>, LIMBS_IN * INDIGITS> exp_rns{ std::array<uint64_t,2>{1,0}, std::array<uint64_t,2>{0,1} };
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> exp_corr{0,0};
    std::array<BigInt, LIMBS_IN * INDIGITS> exp_sqe{ BigInt(15421), BigInt(50413) };

    bool ok = true;
    if (rns_values_tf != exp_rns) {
        std::cerr << "rns_values mismatch\n";
        ok = false;
    }
    if (correction != exp_corr) {
        std::cerr << "correction mismatch\n";
        ok = false;
    }
    if (!(sqe_tf[0] == exp_sqe[0] && sqe_tf[1] == exp_sqe[1])) {
        std::cerr << "shifted_quotient_estimations mismatch\n";
        ok = false;
    }

    if (!ok) {
        std::cout << "FAIL" << std::endl;
        return 1;
    }
    std::cout << "PASS" << std::endl;
    return 0;
}


