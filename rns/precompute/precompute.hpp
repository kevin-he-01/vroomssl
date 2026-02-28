#pragma once
#include "gmp_wrapper.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

template<int limbs>
class GenRNS {
    public:
    std::array<uint64_t, limbs> moduli;
    uint64_t state;
    BigInt modulus;

    GenRNS(BigInt target, int init_state=-1, BigInt prev=BigInt(1), int modbits=52) {
        if (modbits > 64) {
            fprintf(stderr, "Error: modbits must be less than 64\n");
            abort();
        }
        // std::cout << "GenRNS constructor called with target=" << target << ", init_state=" << init_state << ", prev=" << prev << ", modbits=" << modbits << std::endl;
        state = init_state;
        modulus = BigInt(1);
        BigInt total = modulus * prev;
        int found = 0;
        while (found < limbs) {
            state += 2;
            uint64_t m_candidate = (1ull << modbits) - state;
            BigInt m_big(m_candidate);
            if (total.gcd(m_big) == 1) {
                modulus *= m_big;
                total *= m_big;
                moduli[found] = static_cast<uint64_t>(m_big.to_ulong());
                found++;
            }
        }
        if (total < target) {
            fprintf(stderr, "Error: Target modulus not reached\n");
            abort();
        }
    }
};

template<int limbs, int max_bound_M=9, int max_bound_N=6, int modbits=52>
class RNSModuli {
    public:
    GenRNS<limbs> M;
    GenRNS<limbs> N;

    RNSModuli(BigInt target) : M(target * BigInt(max_bound_M), -1, BigInt(1), modbits), N(target * BigInt(max_bound_N), M.state, M.modulus, modbits) {
    }
};

template<int limbs>
struct RNSModuliStorage {
    static std::unique_ptr<RNSModuli<limbs>> instance;
};

template<int limbs>
std::unique_ptr<RNSModuli<limbs>> RNSModuliStorage<limbs>::instance = nullptr;

template<int limbs, int max_bound_M=9, int max_bound_N=6, int modbits=52>
RNSModuli<limbs, max_bound_M, max_bound_N, modbits> make_RNSModuli(BigInt target) {
    if (RNSModuliStorage<limbs>::instance == nullptr) {
        RNSModuliStorage<limbs>::instance = std::make_unique<RNSModuli<limbs, max_bound_M, max_bound_N, modbits>>(target);
    }
    return *RNSModuliStorage<limbs>::instance;
}

// Helpers analogous to Python rns_helpers.py
template<size_t N>
inline BigInt total_modulus(const std::array<BigInt, N> &moduli) {
    BigInt prod(1);
    for (size_t i = 0; i < N; i++) {
        if (moduli[i].is_zero()) {
            fprintf(stderr, "Error: Modulus 0\n");
            abort();
        }
        prod *= moduli[i];
    }
    return prod;
}

template<size_t N>
inline std::array<BigInt, N> rns_precompute_u64(const std::array<BigInt, N> &moduli) {
    std::array<BigInt, N> pre{};
    BigInt modulus = total_modulus(moduli);
    for (size_t i = 0; i < N; i++) {
        BigInt m = moduli[i];
        BigInt rest = modulus / m;
        BigInt inv = (rest % m).mod_inverse(m);
        BigInt icrt = (rest * inv) % modulus;
        pre[i] = icrt;
    }
    return pre;
}

inline BigInt ceil_div_big(const BigInt &x, const BigInt &y) {
    BigInt q = x / y;
    BigInt r = x % y;
    if (r != 0) q += BigInt(1);
    return q;
}

// Helper to convert a value to radix-RNS representation (to_radix_rns in Python)
// Python: word_reinterpret(r, base, digits) = [(r >> (base*i)) & (2^base - 1) for i in range(digits)]
template<size_t LIMBS_OUT, size_t OUTDIGITS, int OUTBITS>
inline void to_radix_rns(
    const BigInt &value,
    const std::array<BigInt, LIMBS_OUT> &moduli_out,
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> &out
) {
    const BigInt mask_big = (OUTBITS >= 64) ? (BigInt(1) << 64) - 1 : (BigInt(1) << OUTBITS) - 1;
    size_t w = 0;
    for (size_t mo = 0; mo < LIMBS_OUT; mo++) {
        BigInt m = moduli_out[mo];
        BigInt r = value % m;
        // Extract OUTDIGITS words of OUTBITS each from r (matching Python's word_reinterpret)
        // Don't use to_ulong() which truncates - extract words directly from BigInt
        for (size_t di = 0; di < OUTDIGITS; di++) {
            BigInt shifted = r >> (OUTBITS * di);
            BigInt word = shifted & mask_big;
            // Now safely convert to uint64_t (word is at most OUTBITS bits, which is <= 64)
            out[w++] = static_cast<uint64_t>(word.to_ulong());
        }
    }
}

// Full precompute to match scripts/precompute_matrix.py: gen_precompute_wide
// This closely mirrors the Python implementation for debugging
template<size_t LIMBS_IN, size_t LIMBS_OUT, size_t INDIGITS, int INBITS, size_t OUTDIGITS, int OUTBITS, int PRECISION>
inline void gen_precompute_wide(
    const std::array<BigInt, LIMBS_IN> &moduli_in,
    const std::array<BigInt, LIMBS_OUT> &moduli_out,
    const BigInt &target_modulus,
    const BigInt &premult,
    const BigInt &postmult,
    std::array<std::array<uint64_t, LIMBS_OUT * OUTDIGITS>, LIMBS_IN * INDIGITS> &rns_values_tf,
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> &correction,
    std::array<BigInt, LIMBS_IN * INDIGITS> &shifted_quotient_estimations_tf,
    bool debug = false
) {
    // Step 1: Compute modulus and modulus_out (matching Python)
    const BigInt modulus = total_modulus(moduli_in);
    const BigInt modulus_out = total_modulus(moduli_out);
    BigInt tgt = target_modulus;
    if (tgt == BigInt(0)) {
        tgt = modulus_out;
    }
    if (debug) {
        std::cout << "  [DEBUG Step 1] modulus=" << modulus << ", modulus_out=" << modulus_out << ", target=" << tgt << std::endl;
    }

    // Step 2: Compute icrt_factors (matching Python: rns_precompute)
    const auto icrt_factors = rns_precompute_u64(moduli_in);
    if (debug && LIMBS_IN <= 8) {
        std::cout << "  [DEBUG Step 2] icrt_factors=[";
        for (size_t i = 0; i < LIMBS_IN; i++) {
            std::cout << icrt_factors[i];
            if (i < LIMBS_IN - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
    
    // Step 3: Compute inbase and fixed_point (matching Python)
    const BigInt inbase = BigInt(1) << INBITS;
    const BigInt fixed_point = BigInt(1) << PRECISION;
    if (debug) {
        std::cout << "  [DEBUG Step 3] inbase=" << inbase << ", fixed_point=" << fixed_point << std::endl;
    }

    // Step 4: Compute shifted_icrt_factors (matching Python)
    // shifted_icrt_factors = [[(premult * (inbase**j) * i) % modulus for i in icrt_factors] for j in range(indigits)]
    std::array<std::array<BigInt, LIMBS_IN>, INDIGITS> shifted_icrt_factors;
    BigInt power(1);  // Start with inbase**0 = 1
    for (size_t j = 0; j < INDIGITS; j++) {
        for (size_t i = 0; i < LIMBS_IN; i++) {
            BigInt val = (premult * power * icrt_factors[i]) % modulus;
            if (val < BigInt(0)) val += modulus;
            shifted_icrt_factors[j][i] = val;
        }
        if (debug && j == 0 && LIMBS_IN <= 8) {
            std::cout << "  [DEBUG Step 4] shifted_icrt_factors[0]=[";
            for (size_t i = 0; i < LIMBS_IN; i++) {
                std::cout << shifted_icrt_factors[0][i];
                if (i < LIMBS_IN - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }
        // Compute inbase**(j+1) for next iteration
        if (j < INDIGITS - 1) {
            power = (power * inbase) % modulus;
        }
    }

    // Step 5: Compute target_shifted_icrt_factors (matching Python)
    // target_shifted_icrt_factors = [[((f % target_modulus) * postmult) % modulus_out for f in r] for r in shifted_icrt_factors]
    std::array<std::array<BigInt, LIMBS_IN>, INDIGITS> target_shifted_icrt_factors;
    for (size_t j = 0; j < INDIGITS; j++) {
        for (size_t i = 0; i < LIMBS_IN; i++) {
            BigInt f = shifted_icrt_factors[j][i];
            BigInt f_mod_tgt = f % tgt;
            if (f_mod_tgt < BigInt(0)) f_mod_tgt += tgt;
            BigInt val = (f_mod_tgt * postmult) % modulus_out;
            if (val < BigInt(0)) val += modulus_out;
            target_shifted_icrt_factors[j][i] = val;
        }
    }
    if (debug && LIMBS_IN <= 8) {
        std::cout << "  [DEBUG Step 5] target_shifted_icrt_factors[0]=[";
        for (size_t i = 0; i < LIMBS_IN; i++) {
            std::cout << target_shifted_icrt_factors[0][i];
            if (i < LIMBS_IN - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Step 6: Compute shifted_quotient_estimations (matching Python)
    // shifted_quotient_estimations = [[ceil_div(p * fixed_point, modulus) for p in f] for f in shifted_icrt_factors]
    for (size_t j = 0; j < INDIGITS; j++) {
        for (size_t i = 0; i < LIMBS_IN; i++) {
            size_t idx_tf = i * INDIGITS + j;
            BigInt p = shifted_icrt_factors[j][i];
            shifted_quotient_estimations_tf[idx_tf] = ceil_div_big(p * fixed_point, modulus);
        }
    }
    if (debug && LIMBS_IN <= 8) {
        std::cout << "  [DEBUG Step 6] shifted_quotient_estimations[0.." << (LIMBS_IN-1) << "]=[";
        for (size_t i = 0; i < LIMBS_IN; i++) {
            size_t idx_tf = i * INDIGITS + 0;
            std::cout << shifted_quotient_estimations_tf[idx_tf];
            if (i < LIMBS_IN - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Step 7: Compute rns_values (matching Python: to_radix_rns)
    // rns_values = [[to_radix_rns(f, moduli_out, outbits, outdigits) for f in r] for r in target_shifted_icrt_factors]
    for (size_t j = 0; j < INDIGITS; j++) {
        for (size_t i = 0; i < LIMBS_IN; i++) {
            size_t idx_tf = i * INDIGITS + j;
            to_radix_rns<LIMBS_OUT, OUTDIGITS, OUTBITS>(
                target_shifted_icrt_factors[j][i],
                moduli_out,
                rns_values_tf[idx_tf]
            );
        }
    }
    if (debug && LIMBS_IN <= 8 && LIMBS_OUT <= 8) {
        std::cout << "  [DEBUG Step 7] rns_values[0]=[";
        for (size_t w = 0; w < LIMBS_OUT * OUTDIGITS; w++) {
            std::cout << rns_values_tf[0][w];
            if (w < LIMBS_OUT * OUTDIGITS - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Step 8: Compute correction (matching Python)
    // correction = to_radix_rns((((-modulus) % target_modulus) * postmult) % modulus_out, moduli_out, outbits, outdigits)
    BigInt neg_mod = (BigInt(0) - modulus) % tgt;
    if (neg_mod < BigInt(0)) neg_mod += tgt;
    BigInt corr_val = (neg_mod * postmult) % modulus_out;
    if (corr_val < BigInt(0)) corr_val += modulus_out;
    to_radix_rns<LIMBS_OUT, OUTDIGITS, OUTBITS>(corr_val, moduli_out, correction);
    if (debug) {
        std::cout << "  [DEBUG Step 8] neg_mod=" << neg_mod << ", corr_val=" << corr_val << std::endl;
        std::cout << "  [DEBUG Step 8] correction=[";
        for (size_t w = 0; w < LIMBS_OUT * OUTDIGITS; w++) {
            std::cout << correction[w];
            if (w < LIMBS_OUT * OUTDIGITS - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
}

// Minimal precompute to match scripts/precompute_matrix.py: gen_precompute_wide_min
template<size_t LIMBS_IN, size_t LIMBS_OUT, size_t INDIGITS, int INBITS, size_t OUTDIGITS, int OUTBITS, int PRECISION>
inline void gen_precompute_wide_min(
    const std::array<BigInt, LIMBS_IN> &moduli_in,
    const std::array<BigInt, LIMBS_OUT> &moduli_out,
    const BigInt &target_modulus,
    const BigInt &premult,
    const BigInt &postmult,
    std::array<std::array<uint64_t, LIMBS_OUT * OUTDIGITS>, LIMBS_IN * INDIGITS> &rns_values_tf,
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> &correction,
    std::array<BigInt, LIMBS_IN * INDIGITS> &shifted_quotient_estimations_tf
) {
    const BigInt modulus = total_modulus(moduli_in);
    BigInt modulus_out = total_modulus(moduli_out);
    BigInt tgt = target_modulus;
    if (tgt == BigInt(0)) {
        tgt = modulus_out;
    }
    const auto icrt = rns_precompute_u64(moduli_in);
    const BigInt inbase = BigInt(1) << INBITS;
    const BigInt fixed_point = BigInt(1) << PRECISION;

    const BigInt premult_mod = premult % modulus;
    const BigInt postmult_mod_out = postmult % modulus_out;

    for (auto &row : rns_values_tf) row.fill(0);
    for (auto &d : correction) d = 0;
    for (auto &v : shifted_quotient_estimations_tf) v = BigInt(0);

    BigInt power(1);
    const size_t rows = INDIGITS;
    const size_t cols = LIMBS_IN;

    for (size_t j = 0; j < rows; j++) {
        BigInt mul = (premult_mod * power) % modulus;
        for (size_t i = 0; i < cols; i++) {
            BigInt p = (mul * icrt[i]) % modulus;
            size_t idx_tf = i * rows + j;
            shifted_quotient_estimations_tf[idx_tf] = ceil_div_big(p * fixed_point, modulus);

            BigInt tgt_val = ((p % tgt) * postmult_mod_out) % modulus_out;
            size_t w = 0;
            const BigInt mask_big = (OUTBITS >= 64) ? (BigInt(1) << 64) - 1 : (BigInt(1) << OUTBITS) - 1;
            for (size_t mo = 0; mo < LIMBS_OUT; mo++) {
                BigInt m = moduli_out[mo];
                BigInt r = tgt_val % m;
                // Extract OUTDIGITS words directly from BigInt (don't truncate with to_ulong())
                for (size_t di = 0; di < OUTDIGITS; di++) {
                    BigInt shifted = r >> (OUTBITS * di);
                    BigInt word = shifted & mask_big;
                    rns_values_tf[idx_tf][w] = static_cast<uint64_t>(word.to_ulong());
                    w++;
                }
            }
        }
        power = (power * inbase) % modulus;
    }

    BigInt neg_mod = (BigInt(0) - modulus) % tgt;
    if (neg_mod < BigInt(0)) neg_mod += tgt;
    BigInt corr_src = (neg_mod * postmult_mod_out) % modulus_out;
    size_t w = 0;
    // uint64_t corr_mask = (OUTBITS >= 64) ? ~uint64_t(0) : ((uint64_t(1) << OUTBITS) - 1);
    const BigInt mask_big = (OUTBITS >= 64) ? (BigInt(1) << 64) - 1 : (BigInt(1) << OUTBITS) - 1;
    for (size_t mo = 0; mo < LIMBS_OUT; mo++) {
        BigInt m = moduli_out[mo];
        BigInt r = corr_src % m;
        // Extract OUTDIGITS words directly from BigInt (don't truncate with to_ulong())
        for (size_t di = 0; di < OUTDIGITS; di++) {
            BigInt shifted = r >> (OUTBITS * di);
            BigInt word = shifted & mask_big;
            correction[w++] = static_cast<uint64_t>(word.to_ulong());
        }
    }
}
