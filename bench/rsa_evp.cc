// Benchmark for BoringSSL RSA signing using EVP (EVP_PKEY_sign), to match the same benchmarks as OpenSSL.
#include <benchmark/benchmark.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <cstring>

// Generate an RSA key pair of specified bit size
static EVP_PKEY *generate_rsa_key(int bits) {
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  if (!ctx) {
    return NULL;
  }

  if (EVP_PKEY_keygen_init(ctx) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return NULL;
  }

  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return NULL;
  }

  EVP_PKEY *pkey = NULL;
  if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return NULL;
  }

  EVP_PKEY_CTX_free(ctx);
  return pkey;
}

// Generate a SHA-256 digest of random data
static unsigned char *generate_digest(size_t *digest_len) {
  const size_t data_size = 32; // SHA-256 produces 32-byte digest
  unsigned char *digest = (unsigned char *)OPENSSL_malloc(data_size);
  if (!digest) {
    return NULL;
  }

  // Generate random data and hash it
  unsigned char data[256];
  if (!RAND_bytes(data, sizeof(data))) {
    OPENSSL_free(digest);
    return NULL;
  }

  unsigned int hash_len;
  if (!EVP_Digest(data, sizeof(data), digest, &hash_len, EVP_sha256(), NULL)) {
    OPENSSL_free(digest);
    return NULL;
  }

  *digest_len = hash_len;
  return digest;
}
static void BM_RSASign_EVP_PKEY_sign(benchmark::State &state) {
  RSA_set_use_rns(state.range(0));
  int bits = state.range(1);
  EVP_PKEY *pkey = generate_rsa_key(bits);
  if (!pkey) {
    state.SkipWithError("Failed to generate RSA key");
    return;
  }

  size_t digest_len;
  unsigned char *digest = generate_digest(&digest_len);
  if (!digest) {
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to generate digest");
    return;
  }

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (!ctx) {
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to create PKEY context");
    return;
  }

  if (EVP_PKEY_sign_init(ctx) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to initialize sign context");
    return;
  }

  if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to set signature digest");
    return;
  }

  // Determine signature length
  size_t sig_len;
  if (EVP_PKEY_sign(ctx, NULL, &sig_len, digest, digest_len) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to determine signature length");
    return;
  }

  unsigned char *sig = (unsigned char *)OPENSSL_malloc(sig_len);
  if (!sig) {
    EVP_PKEY_CTX_free(ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to allocate signature buffer");
    return;
  }

  {
    size_t current_sig_len = sig_len;
    if (EVP_PKEY_sign(ctx, sig, &current_sig_len, digest, digest_len) <= 0) {
      state.SkipWithError("Failed to sign");
      return;
    }
  }

  for (auto _ : state) {
    size_t current_sig_len = sig_len;
    if (EVP_PKEY_sign(ctx, sig, &current_sig_len, digest, digest_len) <= 0) {
      state.SkipWithError("Failed to sign");
      break;
    }
  }

  OPENSSL_free(sig);
  EVP_PKEY_CTX_free(ctx);
  OPENSSL_free(digest);
  EVP_PKEY_free(pkey);
}

static void BM_RSAVerify_EVP_PKEY_verify(benchmark::State &state) {
  RSA_set_use_rns(state.range(0));
  int bits = state.range(1);
  EVP_PKEY *pkey = generate_rsa_key(bits);
  if (!pkey) {
    state.SkipWithError("Failed to generate RSA key");
    return;
  }

  size_t digest_len;
  unsigned char *digest = generate_digest(&digest_len);
  if (!digest) {
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to generate digest");
    return;
  }

  // Create a signature context for signing
  EVP_PKEY_CTX *sign_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (!sign_ctx) {
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to create sign PKEY context");
    return;
  }

  if (EVP_PKEY_sign_init(sign_ctx) <= 0) {
    EVP_PKEY_CTX_free(sign_ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to initialize sign context");
    return;
  }

  if (EVP_PKEY_CTX_set_signature_md(sign_ctx, EVP_sha256()) <= 0) {
    EVP_PKEY_CTX_free(sign_ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to set signature digest");
    return;
  }

  // Determine signature length and create signature
  size_t sig_len;
  if (EVP_PKEY_sign(sign_ctx, NULL, &sig_len, digest, digest_len) <= 0) {
    EVP_PKEY_CTX_free(sign_ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to determine signature length");
    return;
  }

  unsigned char *sig = (unsigned char *)OPENSSL_malloc(sig_len);
  if (!sig) {
    EVP_PKEY_CTX_free(sign_ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to allocate signature buffer");
    return;
  }

  size_t current_sig_len = sig_len;
  if (EVP_PKEY_sign(sign_ctx, sig, &current_sig_len, digest, digest_len) <= 0) {
    OPENSSL_free(sig);
    EVP_PKEY_CTX_free(sign_ctx);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to create signature");
    return;
  }

  EVP_PKEY_CTX_free(sign_ctx);

  // Create verification context
  EVP_PKEY_CTX *verify_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (!verify_ctx) {
    OPENSSL_free(sig);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to create verify PKEY context");
    return;
  }

  if (EVP_PKEY_verify_init(verify_ctx) <= 0) {
    EVP_PKEY_CTX_free(verify_ctx);
    OPENSSL_free(sig);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to initialize verify context");
    return;
  }

  if (EVP_PKEY_CTX_set_signature_md(verify_ctx, EVP_sha256()) <= 0) {
    EVP_PKEY_CTX_free(verify_ctx);
    OPENSSL_free(sig);
    OPENSSL_free(digest);
    EVP_PKEY_free(pkey);
    state.SkipWithError("Failed to set verify signature digest");
    return;
  }

  if (EVP_PKEY_verify(verify_ctx, sig, current_sig_len, digest, digest_len) <= 0) {
    state.SkipWithError("Failed to verify");
    return;
  }

  for (auto _ : state) {
    if (EVP_PKEY_verify(verify_ctx, sig, current_sig_len, digest, digest_len) <= 0) {
      state.SkipWithError("Failed to verify");
      break;
    }
  }

  EVP_PKEY_CTX_free(verify_ctx);
  OPENSSL_free(sig);
  OPENSSL_free(digest);
  EVP_PKEY_free(pkey);
}

// RNS benchmarks currently disabled due to a bug that I don't have time to fix right now.

BENCHMARK(BM_RSASign_EVP_PKEY_sign)
    ->Args({0, 2048})
    // ->Args({1, 2048})
    ->Args({0, 3072})
    // ->Args({1, 3072})
    ->Args({0, 4096})
    // ->Args({1, 4096})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_RSAVerify_EVP_PKEY_verify)
    ->Args({0, 2048})
    // ->Args({1, 2048})
    ->Args({0, 3072})
    // ->Args({1, 3072})
    ->Args({0, 4096})
    // ->Args({1, 4096})
    ->Unit(benchmark::kMicrosecond);
