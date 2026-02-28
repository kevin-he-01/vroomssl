#define _POSIX_C_SOURCE 199309L
#include <openssl/bn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

// Benchmark configuration
#define NUM_ITERATIONS 1000
#define WARMUP_ITERATIONS 100

#if !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  pragma message "Likely non-release build"
#endif

#pragma message ("Clang version " __clang_version__)

// Timing utilities
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Benchmark BN_mod_exp_mont_consttime (single exponentiation)
static uint64_t benchmark_mod_exp_mont_consttime(BIGNUM *base, const BIGNUM *exp, 
                                                  const BIGNUM *mod, BN_MONT_CTX *mont, BN_CTX *ctx) {
    BIGNUM *result = BN_new();
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate result BIGNUM\n");
        return 0;
    }

    uint64_t start = get_time_ns();
    if (!BN_mod_exp_mont_consttime(result, base, exp, mod, ctx, mont)) {
        fprintf(stderr, "Error: BN_mod_exp_mont_consttime failed\n");
        BN_free(result);
        return 0;
    }
    uint64_t end = get_time_ns();

    BN_free(result);
    return end - start;
}

// Run benchmark with statistics
static void run_benchmark(const char *name, uint64_t *times, int count, double throughput_multiplier) {
    printf("\n=== Benchmark: %s ===\n", name);
    
    // Calculate statistics
    uint64_t total = 0;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    int valid_count = 0;

    for (int i = 0; i < count; i++) {
        if (times[i] > 0) {
            total += times[i];
            if (times[i] < min) min = times[i];
            if (times[i] > max) max = times[i];
            valid_count++;
        }
    }

    if (valid_count == 0) {
        printf("ERROR: No valid measurements\n");
        return;
    }

    double avg_ns = (double)total / valid_count;
    double avg_us = avg_ns / 1000.0;
    double avg_ms = avg_us / 1000.0;

    printf("Iterations: %d (valid: %d)\n", count, valid_count);
    printf("Total time: %.2f ms\n", total / 1000000.0);
    printf("Average: %.2f ns (%.3f us, %.6f ms)\n", avg_ns, avg_us, avg_ms);
    printf("Min: %lu ns (%.3f us)\n", min, min / 1000.0);
    printf("Max: %lu ns (%.3f us)\n", max, max / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput_multiplier * 1000000000.0 / avg_ns);
}

// Generate a random BIGNUM of specified bit size
static BIGNUM *generate_random_bn(int bits) {
    BIGNUM *bn = BN_new();
    if (!bn) {
        return NULL;
    }

    if (!BN_rand(bn, bits, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY)) {
        BN_free(bn);
        return NULL;
    }

    return bn;
}

// Generate a random modulus (odd number) suitable for Montgomery
static BIGNUM *generate_random_modulus(int bits) {
    BIGNUM *mod = BN_new();
    if (!mod) {
        return NULL;
    }

    // Generate random number
    if (!BN_rand(mod, bits, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY)) {
        BN_free(mod);
        return NULL;
    }

    // Ensure it's odd (set least significant bit) - required for Montgomery
    if (!BN_set_bit(mod, 0)) {
        BN_free(mod);
        return NULL;
    }

    // Set most significant bit to ensure it's exactly `bits` bits long
    // This is crucial to trigger the AVX512IFMA or AVXIFMA code paths.
    // See OpenSSL source code: crypto/bn/bn_exp.c:1471-1477 (tag openssl-3.6.0)
    if (!BN_set_bit(mod, bits - 1)) {
        BN_free(mod);
        return NULL;
    }

    return mod;
}

// Create Montgomery context for a modulus
static BN_MONT_CTX *create_montgomery_ctx(BIGNUM *mod, BN_CTX *ctx) {
    BN_MONT_CTX *mont = BN_MONT_CTX_new();
    if (!mont) {
        return NULL;
    }

    if (!BN_MONT_CTX_set(mont, mod, ctx)) {
        BN_MONT_CTX_free(mont);
        return NULL;
    }

    return mont;
}

// Benchmark single exponentiation for a given bit size
static void benchmark_single_exp(int bits, BN_CTX *ctx) {
    // printf("\n=== BN_mod_exp_mont_consttime Benchmark (%d-bit modulus) ===\n", bits);

    // Generate modulus, base, and exponent
    BIGNUM *mod = generate_random_modulus(bits);
    BIGNUM *base = generate_random_bn(bits);
    BIGNUM *exp = generate_random_bn(bits);
    BN_MONT_CTX *mont = NULL;

    if (!mod || !base || !exp) {
        fprintf(stderr, "Error: Failed to generate test values\n");
        if (mod) BN_free(mod);
        if (base) BN_free(base);
        if (exp) BN_free(exp);
        return;
    }

    // Ensure base < modulus
    if (BN_cmp(base, mod) >= 0) {
        BN_mod(base, base, mod, ctx);
    }

    // Create Montgomery context
    mont = create_montgomery_ctx(mod, ctx);
    if (!mont) {
        fprintf(stderr, "Error: Failed to create Montgomery context\n");
        BN_free(mod);
        BN_free(base);
        BN_free(exp);
        return;
    }

    // printf("Modulus bits: %d\n", BN_num_bits(mod));
    // printf("Base bits: %d\n", BN_num_bits(base));
    // printf("Exponent bits: %d\n", BN_num_bits(exp));

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        BIGNUM *result = BN_new();
        if (result) {
            BN_mod_exp_mont_consttime(result, base, exp, mod, ctx, mont);
            BN_free(result);
        }
    }

    // Actual benchmark
    uint64_t *times = malloc(NUM_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        BN_free(mod);
        BN_free(base);
        BN_free(exp);
        BN_MONT_CTX_free(mont);
        return;
    }

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        times[i] = benchmark_mod_exp_mont_consttime(base, exp, mod, mont, ctx);
    }

    run_benchmark("BN_mod_exp_mont_consttime", times, NUM_ITERATIONS, 1.0);

    printf("\n");
    printf("Modulus bits: %d\n", BN_num_bits(mod));
    printf("Base bits: %d\n", BN_num_bits(base));
    printf("Exponent bits: %d\n", BN_num_bits(exp));

    free(times);
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_MONT_CTX_free(mont);
}

// int ossl_rsaz_avx512ifma_eligible(void);

// int ossl_rsaz_avxifma_eligible(void);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    BN_CTX *ctx = BN_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create BN_CTX\n");
        return 1;
    }

    printf("OpenSSL BN Modular Exponentiation Benchmark (constant time)\n");
    printf("===========================================\n");
    printf("Benchmarking BN_mod_exp_mont_consttime and BN_mod_exp_mont_consttime_x2\n");
    printf("Modulus sizes: 1024, 2048, 4096 bits\n\n");

    // Check eligibility for AVX512 instructions
    // if (ossl_rsaz_avx512ifma_eligible() || ossl_rsaz_avxifma_eligible()) {
    //     printf("AVX512IFMA or AVXIFMA eligible\n");
    // } else {
    //     printf("AVX512IFMA or AVXIFMA not eligible\n");
    // }

    // Expect doubled throughput for dual exponentiation on supported hardware

    // Test 1024-bit moduli
    benchmark_single_exp(1024, ctx);
    // Test 1536-bit moduli
    benchmark_single_exp(1536, ctx);
    // Test 2048-bit moduli
    benchmark_single_exp(2048, ctx);
    // Test 4096-bit moduli
    // benchmark_single_exp(4096, ctx);
    // benchmark_dual_exp(4096, ctx); // No code path for 8192-bit moduli in OpenSSL as of 3.6.0, expect same throughput as single exp

    BN_CTX_free(ctx);
    return 0;
}
