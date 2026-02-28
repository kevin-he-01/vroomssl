// A separate file from rns.cc to speed up compile times, since this files contains a lot of benchmarks, slowing down build, especially with aggressive inlining.
#include <benchmark/benchmark.h>
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
static void BM_RNSRingMultiplication(benchmark::State &state) {
  Ring<bits> &ring = GetRing<bits>();
  BigInt &m_big = GetModulus<bits>();
  BigInt a = BigInt::random(m_big);
  BigInt b = BigInt::random(m_big);
  auto a_ring = ring.from_int(a);
  auto b_ring = ring.from_int(b);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a_ring);
    benchmark::DoNotOptimize(b_ring);
    a_ring = ring.mul(a_ring, b_ring);
  }
}

// Generate benchmarks for BM_RNSRingMultiplication from 64 to 4096 bits at 64-bit increments
#define RNS_RING_MULT_SETUP(bitsize) \
  BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, bitsize)->Setup(SetupRing<bitsize>)->Unit(benchmark::kNanosecond);

// Use to speed up compile times
#ifdef INCOMPLETE_BENCHMARKS

// BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, 1024)->Setup(SetupRing<1024>);
// BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, 1536)->Setup(SetupRing<1536>);
// BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, 2048)->Setup(SetupRing<2048>);
// BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, 3072)->Setup(SetupRing<3072>);
// BENCHMARK_TEMPLATE(BM_RNSRingMultiplication, 4096)->Setup(SetupRing<4096>);

RNS_RING_MULT_SETUP(1024)
RNS_RING_MULT_SETUP(1536)
RNS_RING_MULT_SETUP(2048)
RNS_RING_MULT_SETUP(3072)
RNS_RING_MULT_SETUP(4096)

#else

RNS_RING_MULT_SETUP(64)
RNS_RING_MULT_SETUP(128)
RNS_RING_MULT_SETUP(192)
RNS_RING_MULT_SETUP(256)
RNS_RING_MULT_SETUP(320)
RNS_RING_MULT_SETUP(384)
RNS_RING_MULT_SETUP(448)
RNS_RING_MULT_SETUP(512)
RNS_RING_MULT_SETUP(576)
RNS_RING_MULT_SETUP(640)
RNS_RING_MULT_SETUP(704)
RNS_RING_MULT_SETUP(768)
RNS_RING_MULT_SETUP(832)
RNS_RING_MULT_SETUP(896)
RNS_RING_MULT_SETUP(960)
RNS_RING_MULT_SETUP(1024)
RNS_RING_MULT_SETUP(1088)
RNS_RING_MULT_SETUP(1152)
RNS_RING_MULT_SETUP(1216)
RNS_RING_MULT_SETUP(1280)
RNS_RING_MULT_SETUP(1344)
RNS_RING_MULT_SETUP(1408)
RNS_RING_MULT_SETUP(1472)
RNS_RING_MULT_SETUP(1536)
RNS_RING_MULT_SETUP(1600)
RNS_RING_MULT_SETUP(1664)
RNS_RING_MULT_SETUP(1728)
RNS_RING_MULT_SETUP(1792)
RNS_RING_MULT_SETUP(1856)
RNS_RING_MULT_SETUP(1920)
RNS_RING_MULT_SETUP(1984)
RNS_RING_MULT_SETUP(2048)
RNS_RING_MULT_SETUP(2112)
RNS_RING_MULT_SETUP(2176)
RNS_RING_MULT_SETUP(2240)
RNS_RING_MULT_SETUP(2304)
RNS_RING_MULT_SETUP(2368)
RNS_RING_MULT_SETUP(2432)
RNS_RING_MULT_SETUP(2496)
RNS_RING_MULT_SETUP(2560)
RNS_RING_MULT_SETUP(2624)
RNS_RING_MULT_SETUP(2688)
RNS_RING_MULT_SETUP(2752)
RNS_RING_MULT_SETUP(2816)
RNS_RING_MULT_SETUP(2880)
RNS_RING_MULT_SETUP(2944)
RNS_RING_MULT_SETUP(3008)
RNS_RING_MULT_SETUP(3072)
RNS_RING_MULT_SETUP(3136)
RNS_RING_MULT_SETUP(3200)
RNS_RING_MULT_SETUP(3264)
RNS_RING_MULT_SETUP(3328)
RNS_RING_MULT_SETUP(3392)
RNS_RING_MULT_SETUP(3456)
RNS_RING_MULT_SETUP(3520)
RNS_RING_MULT_SETUP(3584)
RNS_RING_MULT_SETUP(3648)
RNS_RING_MULT_SETUP(3712)
RNS_RING_MULT_SETUP(3776)
RNS_RING_MULT_SETUP(3840)
RNS_RING_MULT_SETUP(3904)
RNS_RING_MULT_SETUP(3968)
RNS_RING_MULT_SETUP(4032)
RNS_RING_MULT_SETUP(4096)

#undef RNS_RING_MULT_SETUP

#endif
