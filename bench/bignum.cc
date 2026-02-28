#include <benchmark/benchmark.h>

#include <openssl/bn.h>

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

static void BM_BSSLModMulMontgomery(benchmark::State &state) {
  int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *a = generate_random_bn(bits);
  BIGNUM *b = generate_random_bn(bits);
  
  // Create Montgomery context
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);

  if (!mont) {
    BN_free(mod);
    BN_free(a);
    BN_free(b);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create Montgomery context\n");
    abort();
  }

  // Ensure inputs are less than modulus
  if (BN_cmp(a, mod) >= 0) {
    BN_mod(a, a, mod, ctx);
  }
  if (BN_cmp(b, mod) >= 0) {
    BN_mod(b, b, mod, ctx);
  }

  // Convert inputs to Montgomery form
  BIGNUM *a_mont = BN_new();
  BIGNUM *b_mont = BN_new();
  BIGNUM *result = BN_new();
  
  if (!a_mont || !b_mont || !result) {
    BN_free(a_mont);
    BN_free(b_mont);
    BN_free(result);
    BN_free(mod);
    BN_free(a);
    BN_free(b);
    BN_MONT_CTX_free(mont);
    BN_CTX_free(ctx);
    return;
  }

  BN_to_montgomery(a_mont, a, mont, ctx);
  BN_to_montgomery(b_mont, b, mont, ctx);

  for (auto _ : state) {
    BN_mod_mul_montgomery(result, a_mont, b_mont, mont, ctx);
    benchmark::DoNotOptimize(result);
  }

  BN_free(a_mont);
  BN_free(b_mont);
  BN_free(result);
  BN_free(mod);
  BN_free(a);
  BN_free(b);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

static void BM_BSSLModExpConsttime(benchmark::State &state) {
  int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *base = generate_random_bn(bits);
  BIGNUM *exp = generate_random_bn(bits);
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);

  if (!mont) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create Montgomery context\n");
    abort();
  }

  // Ensure base < modulus
  if (BN_cmp(base, mod) >= 0) {
      BN_mod(base, base, mod, ctx);
  }

  BIGNUM *result = BN_new();
  if (!result) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create result BIGNUM\n");
    abort();
  }

  for (auto _ : state) {
    BN_mod_exp_mont_consttime(result, base, exp, mod, ctx, mont);
  }

  BN_free(mod);
  BN_free(base);
  BN_free(exp);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

static void BM_BSSLModExpRSAE(benchmark::State &state) {
  int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *base = generate_random_bn(bits);
  BIGNUM *exp = BN_new();
  BN_set_word(exp, 65537);
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);

  if (!mont) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create Montgomery context\n");
    abort();
  }

  // Ensure base < modulus
  if (BN_cmp(base, mod) >= 0) {
      BN_mod(base, base, mod, ctx);
  }

  BIGNUM *result = BN_new();
  if (!result) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create result BIGNUM\n");
    abort();
  }

  for (auto _ : state) {
    BN_mod_exp_mont(result, base, exp, mod, ctx, mont);
    benchmark::DoNotOptimize(result);
  }

  BN_free(mod);
  BN_free(base);
  BN_free(exp);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

static void BM_RNSModExpRSAE(benchmark::State &state) {
  int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *base = generate_random_bn(bits);
  BIGNUM *exp = BN_new();
  BN_set_word(exp, 65537);
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);
  BN_MONT_CTX_set_use_rns(mont, 1);

  if (!mont) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create Montgomery context\n");
    abort();
  }

  // Ensure base < modulus
  if (BN_cmp(base, mod) >= 0) {
      BN_mod(base, base, mod, ctx);
  }

  BIGNUM *result = BN_new();
  if (!result) {
    BN_free(mod);
    BN_free(base);
    BN_free(exp);
    BN_CTX_free(ctx);
    fprintf(stderr, "Failed to create result BIGNUM\n");
    abort();
  }

  for (auto _ : state) {
    BN_mod_exp_mont(result, base, exp, mod, ctx, mont);
    benchmark::DoNotOptimize(result);
  }

  BN_free(mod);
  BN_free(base);
  BN_free(exp);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

BENCHMARK(BM_BSSLModMulMontgomery)
  ->DenseRange(64, 4096, 64)
  ->Unit(benchmark::kNanosecond);
BENCHMARK(BM_BSSLModExpConsttime)
  ->Arg(1024)
  ->Arg(1536)
  ->Arg(2048)
  ->Arg(3072)
  ->Arg(4096)
  ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_BSSLModExpRSAE)
  ->Arg(1024)
  ->Arg(1536)
  ->Arg(2048)
  ->Arg(3072)
  ->Arg(4096)
  ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_RNSModExpRSAE)
  ->Arg(1024)
  ->Arg(1536)
  ->Arg(2048)
  ->Arg(3072)
  ->Arg(4096)
  ->Unit(benchmark::kMicrosecond);
