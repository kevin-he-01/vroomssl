#include <benchmark/benchmark.h>
#include <openssl/bn.h>
#include "../rns/crypto/drns_ring.hpp"
#include "../rns/crypto/exponentiation.hpp"
#include "../rns/crypto/common.hpp"

#define Ring DRNSRing

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

// Global storage for pre-setup Ring objects
template <int bits>
struct RingStorage {
  static std::unique_ptr<Ring<bits>> ring;
  static BigInt modulus;
};

template <int bits>
std::unique_ptr<Ring<bits>> RingStorage<bits>::ring = nullptr;

template <int bits>
BigInt RingStorage<bits>::modulus = BigInt();

// Setup function to initialize Ring objects for each bit size
template <int bits>
static void SetupRing(const benchmark::State &state) {
  if (RingStorage<bits>::ring == nullptr) {
    BIGNUM *mod = generate_random_prime(bits);
    RingStorage<bits>::modulus = BigInt(mod);
    BN_free(mod);
    RingStorage<bits>::ring =
        std::make_unique<Ring<bits>>(RingStorage<bits>::modulus);
  }
}

// Explicit setup functions for each bit size used in benchmarks
static void SetupRing1024(const benchmark::State &state) {
  SetupRing<1024>(state);
}
static void SetupRing1536(const benchmark::State &state) {
  SetupRing<1536>(state);
}
static void SetupRing2048(const benchmark::State &state) {
  SetupRing<2048>(state);
}
static void SetupRing3072(const benchmark::State &state) {
  SetupRing<3072>(state);
}
static void SetupRing4096(const benchmark::State &state) {
  SetupRing<4096>(state);
}

// Helper function to get the pre-setup Ring object
template <int bits>
static Ring<bits> &GetRing() {
  return *RingStorage<bits>::ring;
}

template <int bits>
static BigInt &GetModulus() {
  return RingStorage<bits>::modulus;
}

template <int bits>
static void BM_DRNSRingConstructor(benchmark::State &state) {
  BIGNUM *mod = generate_random_prime(bits);
  BigInt m_big(mod);
  BN_free(mod);
  for (auto _ : state) {
    benchmark::DoNotOptimize(m_big);
    Ring<bits> ring(m_big);
    benchmark::DoNotOptimize(&ring);
  }
}

template <int bits>
static void BM_DRNSInt2Mont(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    auto result = ring.from_int(a);
    benchmark::DoNotOptimize(result);
  }
}

template <int bits>
static void BM_DRNSMont2Int(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  auto a_ring = ring.from_int(a);
  for (auto _ : state) {
    benchmark::DoNotOptimize(&a_ring);
    BigInt result = ring.to_int(a_ring);
    benchmark::DoNotOptimize(result);
  }
}

template <int bits, typename Variant>
static void BM_DRNSRingMultiplication(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  BigInt b = BigInt::random(m_big);
  auto a_ring = ring.from_int(a);
  auto b_ring = ring.from_int(b);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_ring);
    benchmark::DoNotOptimize(b_ring);
    a_ring = ring.template mul<Variant>(a_ring, b_ring);
  }
}

// template <int bits>
// static void BM_DRNSRingMultiplicationWarmup(benchmark::State &state) {
//   Ring<bits> &ring = GetRing<bits>();
//   BigInt &m_big = GetModulus<bits>();
//   BigInt a = BigInt::random(m_big);
//   BigInt b = BigInt::random(m_big);
//   auto a_ring = ring.from_int(a);
//   auto b_ring = ring.from_int(b);
//   for (int i = 0; i < 20000; i++) {
//     a_ring = ring.mul(a_ring, b_ring);
//   }
//   for (auto _ : state) {
//     benchmark::DoNotOptimize(a_ring);
//     benchmark::DoNotOptimize(b_ring);
//     a_ring = ring.mul(a_ring, b_ring);
//   }
// }


template <int bits>
static void BM_DRNSRingMultiplicationBatched(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big);
  BigInt a2 = BigInt::random(m_big);
  BigInt b1 = BigInt::random(m_big);
  BigInt b2 = BigInt::random(m_big);
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  auto b1_ring = ring.from_int(b1);
  auto b2_ring = ring.from_int(b2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<typename Ring<bits>::RingElement, 2> b_rings = {b1_ring, b2_ring};
  // a_rings = b_rings;
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(b_rings);
    a_rings = ring.template batch_mulreduce<2>(a_rings, b_rings);
  }
}

template <int bits>
static void BM_DRNSDifferentRingMultiplicationBatched(benchmark::State &state) {
  Ring<bits> &ring1 = GetRing<bits>();
  BigInt &m_big1 = GetModulus<bits>();
  BIGNUM *mod2 = generate_random_prime(bits);
  BigInt m_big2(mod2);
  Ring<bits> ring2(m_big2);
  BN_free(mod2);
  BigInt a1 = BigInt::random(m_big1);
  BigInt a2 = BigInt::random(m_big2);
  BigInt b1 = BigInt::random(m_big1);
  BigInt b2 = BigInt::random(m_big2);
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring2.from_int(a2);
  auto b1_ring = ring1.from_int(b1);
  auto b2_ring = ring2.from_int(b2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<typename Ring<bits>::RingElement, 2> b_rings = {b1_ring, b2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(b_rings);
    a_rings = Ring<bits>::template true_batch_mulreduce<2>(
        rings, a_rings, b_rings);
  }
}

template <int bits>
static void BM_DRNSSameRingMultiplicationBatched(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big);
  BigInt a2 = BigInt::random(m_big);
  BigInt b1 = BigInt::random(m_big);
  BigInt b2 = BigInt::random(m_big);
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  auto b1_ring = ring.from_int(b1);
  auto b2_ring = ring.from_int(b2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<typename Ring<bits>::RingElement, 2> b_rings = {b1_ring, b2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring, &ring};
  // a_rings = b_rings;
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(b_rings);
    a_rings =
        Ring<bits>::template true_batch_mulreduce<2>(rings, a_rings, b_rings);
  }
}

template <int bits, int window_bits>
static void BM_DRNSExponentiation(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  BigInt p = BigInt::random(m_big);
  ExponentScalar<bits> exp_scalar(p - 1);
  auto a_ring = ring.from_int(a);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_ring);
    benchmark::DoNotOptimize(exp_scalar);
    a_ring = fixed_window_exponentiate<Ring<bits>, window_bits, bits>(
        a_ring, exp_scalar, ring);
  }
}

template <int bits, int window_bits>
static void BM_DRNSExponentiationBatched(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big);
  BigInt a2 = BigInt::random(m_big);
  BigInt e1 = BigInt::random(m_big);
  BigInt e2 = BigInt::random(m_big);
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1),
                                                     ExponentScalar<bits>(e2)};
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings =
        fixed_window_exponentiate_batched<Ring<bits>, window_bits, bits, 2>(
            a_rings, exp_scalars, ring);
  }
}

template <int bits, int window_bits>
static void BM_DRNSDifferentRingExponentiationBatched(benchmark::State &state) {
  Ring<bits> &ring1 = GetRing<bits>();
  BigInt &m_big1 = GetModulus<bits>();
  BIGNUM *mod2 = generate_random_prime(bits);
  BigInt m_big2(mod2);
  Ring<bits> ring2(m_big2);
  BN_free(mod2);
  BigInt a1 = BigInt::random(m_big1);
  BigInt a2 = BigInt::random(m_big2);
  BigInt e1 = BigInt::random(m_big1);
  BigInt e2 = BigInt::random(m_big2);
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1),
                                                     ExponentScalar<bits>(e2)};
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring2.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings = fixed_window_exponentiate_batched_diffring<
        Ring<bits>, window_bits, bits, 2, true, false>(
        a_rings, exp_scalars, rings);
  }
}

template <int bits, int window_bits>
static void BM_DRNSSameRingExponentiationBatched(benchmark::State &state) {
  Ring<bits> &ring1 = GetRing<bits>();
  BigInt &m_big1 = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big1);
  BigInt a2 = BigInt::random(m_big1);
  BigInt e1 = BigInt::random(m_big1);
  BigInt e2 = BigInt::random(m_big1);
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1),
                                                     ExponentScalar<bits>(e2)};
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring1.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring1};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings = fixed_window_exponentiate_batched_diffring<Ring<bits>,
                                                         window_bits, bits, 2>(
        a_rings, exp_scalars, rings);
  }
}

BENCHMARK_TEMPLATE(BM_DRNSRingConstructor, 1024);
BENCHMARK_TEMPLATE(BM_DRNSRingConstructor, 1536);
BENCHMARK_TEMPLATE(BM_DRNSRingConstructor, 2048);

BENCHMARK_TEMPLATE(BM_DRNSInt2Mont, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSInt2Mont, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSInt2Mont, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSInt2Mont, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSInt2Mont, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSMont2Int, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSMont2Int, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSMont2Int, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSMont2Int, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSMont2Int, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1024, DRNSVariant<false, false>)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1536, DRNSVariant<false, false>)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 2048, DRNSVariant<false, false>)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 3072, DRNSVariant<false, false>)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 4096, DRNSVariant<false, false>)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1024, DRNSVariant<true, false>)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1536, DRNSVariant<true, false>)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 2048, DRNSVariant<true, false>)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 3072, DRNSVariant<true, false>)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 4096, DRNSVariant<true, false>)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1024, DRNSVariant<false, true>)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 1536, DRNSVariant<false, true>)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 2048, DRNSVariant<false, true>)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 3072, DRNSVariant<false, true>)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplication, 4096, DRNSVariant<false, true>)->Setup(SetupRing4096);

// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationWarmup, 1024)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationWarmup, 1536)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationWarmup, 2048)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationWarmup, 3072)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationWarmup, 4096)->Setup(SetupRing4096);

// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationBatched, 1024)
//     ->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationBatched, 1536)
//     ->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationBatched, 2048)
//     ->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationBatched, 3072)
//     ->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_DRNSRingMultiplicationBatched, 4096)
//     ->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSSameRingMultiplicationBatched, 1024)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSSameRingMultiplicationBatched, 1536)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSSameRingMultiplicationBatched, 2048)
    ->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSSameRingMultiplicationBatched, 3072)
    ->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSSameRingMultiplicationBatched, 4096)
    ->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingMultiplicationBatched, 1024)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingMultiplicationBatched, 1536)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingMultiplicationBatched, 2048)
    ->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingMultiplicationBatched, 3072)
    ->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingMultiplicationBatched, 4096)
    ->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_DRNSSameRingExponentiationBatched, 1024, 4)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSSameRingExponentiationBatched, 1536, 4)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSSameRingExponentiationBatched, 2048, 4)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 1)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 2)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 3)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 5)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 6)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 7)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1024, 8)->Setup(SetupRing1024);

BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 1)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 2)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 3)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 5)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 6)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 7)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 1536, 8)->Setup(SetupRing1536);

BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 1)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 2)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 3)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 4)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 5)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 6)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 7)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 2048, 8)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 1)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 2)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 3)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 4)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 5)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 6)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 7)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 3072, 8)->Setup(SetupRing3072);

BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 1)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 2)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 3)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 4)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 5)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 6)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 7)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_DRNSExponentiation, 4096, 8)->Setup(SetupRing4096);

// BENCHMARK_TEMPLATE(BM_DRNSExponentiationBatched, 1024, 4)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_DRNSExponentiationBatched, 1536, 4)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_DRNSExponentiationBatched, 2048, 4)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 2)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 2)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 2)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 3)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 3)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 3)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 4)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 4)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 4)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 5)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 5)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 5)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 6)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 6)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 6)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 7)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 7)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 7)
    ->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1024, 8)
    ->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 1536, 8)
    ->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_DRNSDifferentRingExponentiationBatched, 2048, 8)
    ->Setup(SetupRing2048);
