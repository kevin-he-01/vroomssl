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

static void BM_ModExpConsttimeOG(benchmark::State &state) {int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *base = generate_random_bn(bits);
  BIGNUM *exp = generate_random_bn(bits);
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);

  BN_MONT_CTX_set_use_rns(mont, 0);

  // Ensure base < modulus
  if (BN_cmp(base, mod) >= 0) {
      BN_mod(base, base, mod, ctx);
  }

  for (auto _ : state) {
    BIGNUM *result = BN_new();
    if (result) {
        BN_mod_exp_mont_consttime(result, base, exp, mod, ctx, mont);
        BN_free(result);
    }
  }

  BN_free(mod);
  BN_free(base);
  BN_free(exp);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

static void BM_ModExpConsttimeRNS(benchmark::State &state) {
  int bits = state.range(0);
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *mod = generate_random_prime(bits);
  BIGNUM *base = generate_random_bn(bits);
  BIGNUM *exp = generate_random_bn(bits);
  BN_MONT_CTX *mont = create_montgomery_ctx(mod, ctx);

  BN_MONT_CTX_set_use_rns(mont, 1);
  
  // Ensure base < modulus
  if (BN_cmp(base, mod) >= 0) {
      BN_mod(base, base, mod, ctx);
  }

  for (auto _ : state) {
    BIGNUM *result = BN_new();
    if (result) {
        BN_mod_exp_mont_consttime(result, base, exp, mod, ctx, mont);
        BN_free(result);
    }
  }

  BN_free(mod);
  BN_free(base);
  BN_free(exp);
  BN_MONT_CTX_free(mont);
  BN_CTX_free(ctx);
}

BENCHMARK(BM_ModExpConsttimeOG)
    // ->Arg(1024-128)
    ->Arg(1024)
    // ->Arg(1536-128)
    ->Arg(1536)
    // ->Arg(2048-128)
    ->Arg(2048)
    // ->Arg(3072-128)
    // ->Arg(3072)
    // ->Arg(4096-128)
    // ->Arg(4096)
;

BENCHMARK(BM_ModExpConsttimeRNS)
    ->Arg(1024)
    ->Arg(1536)
    ->Arg(2048)
    // ->Arg(3072)
    // ->Arg(4096)
;
