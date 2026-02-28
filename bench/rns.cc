#include <benchmark/benchmark.h>
#include "../rns/crypto/exponentiation.hpp"
#include "../rns/crypto/ring.hpp"
#include <openssl/bn.h>
#include <memory>

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
static void SetupRing(const benchmark::State& state) {
    if (RingStorage<bits>::ring == nullptr) {
        BIGNUM *mod = generate_random_prime(bits);
        RingStorage<bits>::modulus = BigInt(mod);
        BN_free(mod);
        RingStorage<bits>::ring = std::make_unique<Ring<bits>>(RingStorage<bits>::modulus);
    }
}

// Explicit setup functions for each bit size used in benchmarks
static void SetupRing1024(const benchmark::State& state) { SetupRing<1024>(state); }
static void SetupRing1536(const benchmark::State& state) { SetupRing<1536>(state); }
static void SetupRing2048(const benchmark::State& state) { SetupRing<2048>(state); }
static void SetupRing3072(const benchmark::State& state) { SetupRing<3072>(state); }
static void SetupRing4096(const benchmark::State& state) { SetupRing<4096>(state); }

// Helper function to get the pre-setup Ring object
template <int bits>
static Ring<bits>& GetRing() {
    return *RingStorage<bits>::ring;
}

template <int bits>
static BigInt& GetModulus() {
    return RingStorage<bits>::modulus;
}

template <int bits>
static void BM_RNSRingConstructor(benchmark::State &state) {
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
static void BM_RNSInt2Mont(benchmark::State &state) {
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
static void BM_RNSMont2Int(benchmark::State &state) {
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

// For BM_RNSRingMultiplication, see rns_mul.cc (separate files used to speed up compile times)

template <int bits>
static void BM_RNSRingMultiplicationBatched(benchmark::State &state) {
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

template <int bits, bool serialize_reduce = false>
static void BM_RNSDifferentRingMultiplicationBatched(benchmark::State &state) {
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
    a_rings = Ring<bits>::template true_batch_mulreduce<2, serialize_reduce>(rings, a_rings, b_rings);
  }
}

template <int bits>
static void BM_RNSSameRingMultiplicationBatched(benchmark::State &state) {
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
    a_rings = Ring<bits>::template true_batch_mulreduce<2>(rings, a_rings, b_rings);
  }
}

template <int bits, int window_bits>
static void BM_RNSExponentiation(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  BigInt p = BigInt::random(m_big);
  ExponentScalar<bits> exp_scalar(p - 1);
  auto a_ring = ring.from_int(a);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_ring);
    benchmark::DoNotOptimize(exp_scalar);
    a_ring = fixed_window_exponentiate<Ring<bits>, window_bits, bits>(a_ring, exp_scalar, ring);
  }
}

template <int bits, int window_bits>
static void BM_RNSExponentiationBatched(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big);
  BigInt a2 = BigInt::random(m_big);
  BigInt e1 = BigInt::random(m_big);
  BigInt e2 = BigInt::random(m_big);
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1), ExponentScalar<bits>(e2)};
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings = fixed_window_exponentiate_batched<Ring<bits>, window_bits, bits, 2>(a_rings, exp_scalars, ring);
  }
}

template <int bits, int window_bits, int serialize_reduce = false>
static void BM_RNSDifferentRingExponentiationBatched(benchmark::State &state) {
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
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1), ExponentScalar<bits>(e2)};
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring2.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings = fixed_window_exponentiate_batched_diffring<Ring<bits>, window_bits, bits, 2, true, serialize_reduce>(a_rings, exp_scalars, rings);
  }
}

template <int bits, int window_bits>
static void BM_RNSSameRingExponentiationBatched(benchmark::State &state) {
  Ring<bits> &ring1 = GetRing<bits>();
  BigInt &m_big1 = GetModulus<bits>();
  BigInt a1 = BigInt::random(m_big1);
  BigInt a2 = BigInt::random(m_big1);
  BigInt e1 = BigInt::random(m_big1);
  BigInt e2 = BigInt::random(m_big1);
  std::array<ExponentScalar<bits>, 2> exp_scalars = {ExponentScalar<bits>(e1), ExponentScalar<bits>(e2)};
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring1.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring1};
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_rings);
    benchmark::DoNotOptimize(exp_scalars);
    a_rings = fixed_window_exponentiate_batched_diffring<Ring<bits>, window_bits, bits, 2>(a_rings, exp_scalars, rings);
  }
}

template <int bits, int window_bits>
static void BM_FixedWindowTable_ctime_select(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt rand = BigInt::random(m_big);
  auto rand_ring = ring.from_int(rand);
  FixedWindowTable<Ring<bits>, window_bits> fixed_window_table(rand_ring, ring);
  for (auto _ : state) {
    benchmark::DoNotOptimize(fixed_window_table);
    auto result = fixed_window_table.ctime_select(0);
    benchmark::DoNotOptimize(result);
  }
}

// template <int bits, int window_bits>
// static void BM_FixedWindowTableBatched_ctime_select(benchmark::State &state) {
//   Ring<bits> &ring = GetRing<bits>();
//   BigInt &m_big = GetModulus<bits>();
//   BigInt rand1 = BigInt::random(m_big);
//   BigInt rand2 = BigInt::random(m_big);
//   auto rand1_ring = ring.from_int(rand1);
//   auto rand2_ring = ring.from_int(rand2);
//   std::array<typename Ring<bits>::RingElement, 2> base = {rand1_ring, rand2_ring};
//   FixedWindowTableBatched<Ring<bits>, window_bits, 2> fixed_window_table_batched(base, ring);
//   std::array<int, 2> indices = {0, 0};
//   for (auto _ : state) {
//     benchmark::DoNotOptimize(fixed_window_table_batched);
//     auto result = fixed_window_table_batched.ctime_select(indices);
//     benchmark::DoNotOptimize(result);
//   }
// }

template <int bits, int window_bits>
static void BM_FixedWindowTableBatchedMultiRing_ctime_select(benchmark::State &state) {
  Ring<bits> &ring1 = GetRing<bits>();
  BigInt &m_big1 = GetModulus<bits>();
  BIGNUM *mod2 = generate_random_prime(bits);
  BigInt m_big2(mod2);
  Ring<bits> ring2(m_big2);
  BN_free(mod2);
  BigInt rand1 = BigInt::random(m_big1);
  BigInt rand2 = BigInt::random(m_big2);
  auto rand1_ring = ring1.from_int(rand1);
  auto rand2_ring = ring2.from_int(rand2);
  std::array<typename Ring<bits>::RingElement, 2> base = {rand1_ring, rand2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  FixedWindowTableBatchedMultiRing<Ring<bits>, window_bits, 2> fixed_window_table_batched_multiring(base, rings);
  std::array<int, 2> indices = {0, 0};
  for (auto _ : state) {
    benchmark::DoNotOptimize(fixed_window_table_batched_multiring);
    auto result = fixed_window_table_batched_multiring.ctime_select(indices);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_TEMPLATE(BM_RNSRingConstructor, 1024);
BENCHMARK_TEMPLATE(BM_RNSRingConstructor, 1536);
BENCHMARK_TEMPLATE(BM_RNSRingConstructor, 2048);

BENCHMARK_TEMPLATE(BM_RNSInt2Mont, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSInt2Mont, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSInt2Mont, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSInt2Mont, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSInt2Mont, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSMont2Int, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSMont2Int, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSMont2Int, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSMont2Int, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSMont2Int, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSRingMultiplicationBatched, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSRingMultiplicationBatched, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSRingMultiplicationBatched, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSRingMultiplicationBatched, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSRingMultiplicationBatched, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSSameRingMultiplicationBatched, 1024)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSSameRingMultiplicationBatched, 1536)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSSameRingMultiplicationBatched, 2048)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSSameRingMultiplicationBatched, 3072)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSSameRingMultiplicationBatched, 4096)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 1024, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 1536, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 2048, false)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 3072, false)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 4096, false)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 1024, true)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 1536, true)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 2048, true)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 3072, true)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingMultiplicationBatched, 4096, true)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSSameRingExponentiationBatched, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSSameRingExponentiationBatched, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSSameRingExponentiationBatched, 2048, 4)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 1)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 2)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 3)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 5)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 6)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 7)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1024, 8)->Setup(SetupRing1024);

BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 1)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 2)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 3)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 5)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 6)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 7)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 1536, 8)->Setup(SetupRing1536);

BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 1)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 2)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 3)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 4)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 5)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 6)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 7)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 2048, 8)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 1)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 2)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 3)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 4)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 5)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 6)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 7)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 3072, 8)->Setup(SetupRing3072);

BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 1)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 2)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 3)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 4)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 5)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 6)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 7)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTable_ctime_select, 4096, 8)->Setup(SetupRing4096);

// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 1)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 2)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 3)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 4)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 5)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 6)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 7)->Setup(SetupRing1024);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1024, 8)->Setup(SetupRing1024);

// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 1)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 2)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 3)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 4)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 5)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 6)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 7)->Setup(SetupRing1536);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 1536, 8)->Setup(SetupRing1536);

// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 1)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 2)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 3)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 4)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 5)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 6)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 7)->Setup(SetupRing2048);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 2048, 8)->Setup(SetupRing2048);

// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 1)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 2)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 3)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 4)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 5)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 6)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 7)->Setup(SetupRing3072);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 3072, 8)->Setup(SetupRing3072);

// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 1)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 2)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 3)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 4)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 5)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 6)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 7)->Setup(SetupRing4096);
// BENCHMARK_TEMPLATE(BM_FixedWindowTableBatched_ctime_select, 4096, 8)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 1)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 2)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 3)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 5)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 6)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 7)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1024, 8)->Setup(SetupRing1024);

BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 1)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 2)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 3)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 5)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 6)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 7)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 1536, 8)->Setup(SetupRing1536);

BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 1)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 2)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 3)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 4)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 5)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 6)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 7)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 2048, 8)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 1)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 2)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 3)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 4)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 5)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 6)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 7)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 3072, 8)->Setup(SetupRing3072);

BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 1)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 2)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 3)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 4)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 5)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 6)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 7)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_FixedWindowTableBatchedMultiRing_ctime_select, 4096, 8)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 1)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 2)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 3)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 5)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 6)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 7)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1024, 8)->Setup(SetupRing1024);

BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 1)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 2)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 3)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 5)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 6)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 7)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 1536, 8)->Setup(SetupRing1536);

BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 1)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 2)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 3)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 4)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 5)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 6)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 7)->Setup(SetupRing2048);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 2048, 8)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 1)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 2)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 3)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 4)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 5)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 6)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 7)->Setup(SetupRing3072);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 3072, 8)->Setup(SetupRing3072);

BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 1)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 2)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 3)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 4)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 5)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 6)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 7)->Setup(SetupRing4096);
BENCHMARK_TEMPLATE(BM_RNSExponentiation, 4096, 8)->Setup(SetupRing4096);

BENCHMARK_TEMPLATE(BM_RNSExponentiationBatched, 1024, 4)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSExponentiationBatched, 1536, 4)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSExponentiationBatched, 2048, 4)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 2, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 2, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 2, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 2, true)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 2, true)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 2, true)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 3, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 3, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 3, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 3, true)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 3, true)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 3, true)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 4, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 4, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 4, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 4, true)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 4, true)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 4, true)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 5, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 5, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 5, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 6, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 6, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 6, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 7, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 7, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 7, false)->Setup(SetupRing2048);

BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1024, 8, false)->Setup(SetupRing1024);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 1536, 8, false)->Setup(SetupRing1536);
BENCHMARK_TEMPLATE(BM_RNSDifferentRingExponentiationBatched, 2048, 8, false)->Setup(SetupRing2048);
