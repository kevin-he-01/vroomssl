#include "../rns/precompute/wrappers/gmp_wrapper.hpp"
#include "../rns/fmpz_class.hh"
#include <benchmark/benchmark.h>

#define BigInt gmp_wrapper::BigInt

static BigInt generate_random_bigint(int bits) {
    return BigInt::random(BigInt(1) << bits);
}

static void BM_BigInt2BIGNUM(benchmark::State &state) {
    BigInt a = generate_random_bigint(state.range(0));
    BIGNUM *b = BN_new();
    for (auto _ : state) {
        a.to_bn(b);
        benchmark::DoNotOptimize(b);
    }
    BN_free(b);
}

static void BM_BIGNUM2BigInt(benchmark::State &state) {
    BIGNUM *b = BN_new();
    BN_rand(b, state.range(0), BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
    for (auto _ : state) {
        BigInt a(b);
        benchmark::DoNotOptimize(a);
    }
    BN_free(b);
}

static void BM_BigInt2FMPZ(benchmark::State &state) {
    BigInt a = generate_random_bigint(state.range(0));
    FMPZ b;
    for (auto _ : state) {
        a.to_fmpz(b);
        benchmark::DoNotOptimize(b);
    }
}

static void BM_FMPZ2BigInt(benchmark::State &state) {
    BigInt a = generate_random_bigint(state.range(0));
    FMPZ b;
    a.to_fmpz(b);
    for (auto _ : state) {
        BigInt c(b);
        benchmark::DoNotOptimize(c);
    }
}

static void BM_BigIntAdd(benchmark::State &state) {
    BigInt a = generate_random_bigint(state.range(0));
    BigInt b = generate_random_bigint(state.range(0));
    for (auto _ : state) {
        a = a + b;
        benchmark::DoNotOptimize(a);
    }
}

BENCHMARK(BM_BigInt2BIGNUM)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096);
BENCHMARK(BM_BIGNUM2BigInt)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096);
BENCHMARK(BM_BigInt2FMPZ)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096);
BENCHMARK(BM_FMPZ2BigInt)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096);
BENCHMARK(BM_BigIntAdd)->Arg(1024)->Arg(1536)->Arg(2048)->Arg(3072)->Arg(4096);
