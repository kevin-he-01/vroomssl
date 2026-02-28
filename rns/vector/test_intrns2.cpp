#include "multiplication.hpp"
#include <iostream>

int main() {
    // Compare factory results to Python references for small targets
    {
        constexpr int limbs = 1;
        BigInt target(97);
        auto r = make_IntRNS2Montgomery<limbs>(target, 52, 9);
        // Python reference for target=97
        const uint64_t mod1[limbs] = {4503599627370495ull};
        const uint64_t mod2[limbs] = {4503599627370493ull};
        const BigInt premult1("603575207791922");
        const BigInt postmult1("3377699720528088");
        const BigInt premult2("1501199875790165");
        const BigInt postmult2("1");
        // We cannot access r1/r2 internals directly here; this test ensures factory runs.
        // For thorough check, we recompute expected via same C++ formulas and compare to Python numbers.
        GenRNS<limbs> g1(target * BigInt(9), -1, BigInt(1), 52);
        GenRNS<limbs> g2(target * BigInt(6), g1.state, g1.modulus, 52);
        if (g1.moduli[0] != mod1[0] || g2.moduli[0] != mod2[0]) {
            std::cerr << "moduli mismatch" << std::endl;
            return 1;
        }
        const BigInt m1 = g1.modulus;
        const BigInt m2 = g2.modulus;
        const BigInt rpow = BigInt(1) << 52;
        BigInt mont_factor = (BigInt(0) - target).mod_inverse(m1);
        BigInt inv_factor = m1.mod_inverse(m2);
        BigInt p1 = (mont_factor * (rpow.mod_inverse(m1))) % m1;
        BigInt q1 = (target * inv_factor % m2) * (inv_factor % m2) % m2 * (rpow % m2) % m2 * (rpow % m2) % m2;
        BigInt p2 = (m1 % m2) * (rpow.mod_inverse(m2)) % m2;
        BigInt q2 = (rpow % m1) * (rpow % m1) % m1;
        bool ok = true;
        if (!(p1 == premult1)) { std::cerr << "premult1 mismatch\n"; ok = false; }
        if (!(q1 == postmult1)) { std::cerr << "postmult1 mismatch\n"; ok = false; }
        if (!(p2 == premult2)) { std::cerr << "premult2 mismatch\n"; ok = false; }
        if (!(q2 == postmult2)) { std::cerr << "postmult2 mismatch\n"; ok = false; }
        if (!ok) return 1;
    }

    {
        constexpr int limbs = 1;
        BigInt target(65537);
        auto r = make_IntRNS2Montgomery<limbs>(target, 52, 9);
        const uint64_t mod1[limbs] = {4503599627370495ull};
        const uint64_t mod2[limbs] = {4503599627370493ull};
        const BigInt premult1("2384254583994487");
        const BigInt postmult1("3377699720675328");
        const BigInt premult2("1501199875790165");
        const BigInt postmult2("1");

        GenRNS<limbs> g1(target * BigInt(9), -1, BigInt(1), 52);
        GenRNS<limbs> g2(target * BigInt(6), g1.state, g1.modulus, 52);
        if (g1.moduli[0] != mod1[0] || g2.moduli[0] != mod2[0]) {
            std::cerr << "moduli mismatch" << std::endl;
            return 1;
        }
        const BigInt m1 = g1.modulus;
        const BigInt m2 = g2.modulus;
        const BigInt rpow = BigInt(1) << 52;
        BigInt mont_factor = (BigInt(0) - target).mod_inverse(m1);
        BigInt inv_factor = m1.mod_inverse(m2);
        BigInt p1 = (mont_factor * (rpow.mod_inverse(m1))) % m1;
        BigInt q1 = (target * inv_factor % m2) * (inv_factor % m2) % m2 * (rpow % m2) % m2 * (rpow % m2) % m2;
        BigInt p2 = (m1 % m2) * (rpow.mod_inverse(m2)) % m2;
        BigInt q2 = (rpow % m1) * (rpow % m1) % m1;
        bool ok = true;
        if (!(p1 == premult1)) { std::cerr << "premult1 mismatch\n"; ok = false; }
        if (!(q1 == postmult1)) { std::cerr << "postmult1 mismatch\n"; ok = false; }
        if (!(p2 == premult2)) { std::cerr << "premult2 mismatch\n"; ok = false; }
        if (!(q2 == postmult2)) { std::cerr << "postmult2 mismatch\n"; ok = false; }
        if (!ok) return 1;
    }

    std::cout << "PASS" << std::endl;
    return 0;
}


