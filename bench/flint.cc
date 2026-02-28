// https://flintlib.org/doc/mpn_mod.html 
//up to 1024 bits on 64 bit machines
#include <benchmark/benchmark.h>
#include <flint/flint.h>
#include <flint/gr.h>
#include <flint/gr_types.h>
#include <flint/mpn_mod.h>
#include <flint/fmpz_mod.h>
#include <cassert>
#include <flint/fmpz.h>
#include <stdlib.h>
#include <iostream>
#include <chrono>
#include <openssl/bn.h>
#include "../rns/precompute/wrappers/gmp_wrapper.hpp"

#define BigInt gmp_wrapper::BigInt

// Generate a random modulus (prime number) suitable for Montgomery
static BIGNUM *generate_random_prime(int bits) {
    BIGNUM *prime = BN_new();
    if (!prime) {
        return NULL;
    }

    if (!BN_generate_prime_ex(prime, bits, 0, NULL, NULL, NULL)) {
        BN_free(prime);
        return NULL;
    }

    return prime;
}

static FMPZ generate_random_prime_fmpz(int bits) {
    BIGNUM *prime = generate_random_prime(bits);
    BigInt m_big(prime);
    // std::cerr << "m_big: " << m_big.get_decimal_str() << std::endl;
    BN_free(prime);
    FMPZ ret;
    m_big.to_fmpz(ret);
    // flint_printf("ret: %{fmpz}\n", ret.get_fmpz());
    return ret;
}

class MPN {

    public:
    gr_ctx_t ctx;
    MPN(const fmpz_t n, int limbs) {
        int error = gr_ctx_init_mpn_mod(ctx, n);
        // assert(error == GR_SUCCESS);
        if (error != GR_SUCCESS) {
            std::cerr << "Error: gr_ctx_init_mpn_mod failed with error: " << error << std::endl;
            abort();
        }
        // assert(MPN_MOD_CTX_NLIMBS(ctx) == limbs);
        if (MPN_MOD_CTX_NLIMBS(ctx) != limbs) {
            std::cerr << "MPN_MOD_CTX_NLIMBS: " << MPN_MOD_CTX_NLIMBS(ctx) << std::endl;
            std::cerr << "Error: MPN_MOD_CTX_NLIMBS != limbs" << std::endl;
            abort();
        }
    }

    void modmul(nn_srcptr a, nn_srcptr b, nn_ptr result) {
        mpn_mod_mul(result, a, b, ctx);
    }
};

// Larger than 1024 bits
class FMPZRing {

    public:
    fmpz_mod_ctx_t ctx;
    FMPZRing(const fmpz_t n) {
        fmpz_mod_ctx_init(ctx, n);
    }

    void modmul(const fmpz_t a, const fmpz_t b, fmpz_t result) {
        fmpz_mod_mul(result, a, b, ctx);
    }
};

template<int limbs>
int read_arguments(int argc, char** argv, fmpz_t (&values)[3]) {
    assert(argc > 4);
    int num_multiplies = atoi(argv[1]);
    for (int i = 0; i < 3; i++) {
        fmpz_init2(values[i], limbs);
        fmpz_set_str(values[i], argv[i+2], 10);
    }
    return num_multiplies;
}

template<int limbs>
static void BM_FLINTMPN(benchmark::State &state) {
    FMPZ mod_fmpz = generate_random_prime_fmpz(limbs * 64);
    // std::cerr << "mod_fmpz: " << mod_fmpz.get_fmpz() << std::endl;
    FMPZ a = FMPZ::random(mod_fmpz);
    // std::cerr << "a: " << a.get_fmpz() << std::endl;
    FMPZ b = FMPZ::random(mod_fmpz);
    // std::cerr << "b: " << b.get_fmpz() << std::endl;
    MPN m = MPN(mod_fmpz.get_fmpz(), limbs);
    ulong na[limbs];
    ulong nb[limbs];
    mpn_mod_set_fmpz(na, a.get_fmpz(), m.ctx);
    mpn_mod_set_fmpz(nb, b.get_fmpz(), m.ctx);
    for (auto _ : state) {
        ulong tmp[limbs];
        m.modmul(na, nb, tmp);
        for (int j = 0; j < limbs; j++) {
            na[j] = tmp[j];
        }
    }
}

// template <int limbs>
// void test_mpn(fmpz_t a, fmpz_t b, fmpz_t mod_fmpz, int num_multiplies) {
//     MPN m = MPN(mod_fmpz, limbs);
//     ulong na[limbs];
//     ulong nb[limbs];
//     mpn_mod_set_fmpz(na, a, m.ctx);
//     mpn_mod_set_fmpz(nb, b, m.ctx);

//     auto start = std::chrono::high_resolution_clock::now();
//     for (int i = 0; i < num_multiplies; i++) {
//         ulong tmp[limbs];
//         m.modmul(na, nb, tmp);
//         for (int j = 0; j < limbs; j++) {
//             na[j] = tmp[j];
//         }
//     }
//     auto finish = std::chrono::high_resolution_clock::now();
//     std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() << "ns\n";
//     mpn_mod_get_fmpz(a, na, m.ctx);
// }

static void BM_FLINTFMPZRing(benchmark::State &state) {
    FMPZ mod_fmpz = generate_random_prime_fmpz(state.range(0));
    FMPZ a = FMPZ::random(mod_fmpz);
    FMPZ b = FMPZ::random(mod_fmpz);
    FMPZRing m = FMPZRing(mod_fmpz.get_fmpz());
    for (auto _ : state) {
        m.modmul(a.get_fmpz(), b.get_fmpz(), a.get_fmpz());
        benchmark::DoNotOptimize(a);
    }
}

static void BM_FLINTExponentiation(benchmark::State &state) {
    FMPZ mod_fmpz = generate_random_prime_fmpz(state.range(0));
    FMPZ a = FMPZ::random(mod_fmpz);
    FMPZ b = FMPZ::random(mod_fmpz);
    for (auto _ : state) {
        a.powm(a, b, mod_fmpz);
        benchmark::DoNotOptimize(a);
    }
}

static void BM_FLINTExponentiationRSAE(benchmark::State &state) {
    FMPZ mod_fmpz = generate_random_prime_fmpz(state.range(0));
    FMPZ a = FMPZ::random(mod_fmpz);
    FMPZ b = 65537;
    for (auto _ : state) {
        a.powm(a, b, mod_fmpz);
        benchmark::DoNotOptimize(a);
    }
}

// void test_fmpz(fmpz_t a, fmpz_t b, fmpz_t mod_fmpz, int num_multiplies) {
//     FMPZRing m = FMPZRing(mod_fmpz);
//     auto start = std::chrono::high_resolution_clock::now();
//     for (int i = 0; i < num_multiplies; i++) {
//         m.modmul(a, b, a);
//     }
//     auto finish = std::chrono::high_resolution_clock::now();
//     std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() << "ns\n";
// }

// template <int limbs>
// void test(int argc, char** argv) {
//     fmpz_t arguments[3];
//     int num_multiplies = read_arguments<limbs>(argc, argv, arguments);
//     fmpz_t output;
//     if (limbs < MPN_MOD_MAX_LIMBS) {
//         test_mpn<limbs>(arguments[1], arguments[2], arguments[0], num_multiplies);
//     } else {
//         test_fmpz(arguments[1], arguments[2], arguments[0], num_multiplies);
//     }
//     flint_printf("Result: %{fmpz}\n", arguments[1]);
// }

// int main(int argc, char** argv) {
//     test<PYREPLACE_LIMBS1>(argc, argv);
// }

static_assert(MPN_MOD_MAX_LIMBS == 16, "Assumed in this benchmark");

// BENCHMARK(BM_FLINTMPN<1>);
BENCHMARK(BM_FLINTMPN<2>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<3>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<4>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<5>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<6>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<7>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<8>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<9>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<10>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<11>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<12>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<13>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<14>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<15>)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTMPN<16>)->Unit(benchmark::kNanosecond);

BENCHMARK(BM_FLINTFMPZRing)->DenseRange(64, 4096, 64)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_FLINTExponentiation)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FLINTExponentiationRSAE)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096)->Unit(benchmark::kMicrosecond);
