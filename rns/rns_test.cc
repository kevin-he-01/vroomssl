#include <gtest/gtest.h>
#include "../rns/crypto/exponentiation.hpp"
#include "../rns/crypto/ring.hpp"
#include "../rns/vector/conversion.hpp"
#include <openssl/bn.h>

// Generate a random modulus (prime number) suitable for Montgomery
static BIGNUM *generate_random_prime_bignum(int bits) {
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

static BigInt generate_random_prime(int bits) {
  BIGNUM *mod = generate_random_prime_bignum(bits);
  BigInt m_big(mod);
  BN_free(mod);
  return m_big;
}

template <int bits>
static void test_to_int() {
  BigInt m = generate_random_prime(bits);
  Ring<bits> ring(m);
  for (int i = 0; i < 10000; i++) {
    BigInt a = BigInt::random(m);
    auto a_ring = ring.from_int(a);
    BigInt a_actual = ring.to_int(a_ring);
    EXPECT_EQ(a, a_actual);
  }
}

// TODO: test_to_int() but with redundancies in the RNS form (multiples of p)

template <int bits>
static void test_ring_multiplication() {
  BigInt m = generate_random_prime(bits);
  Ring<bits> ring(m);
  for (int i = 0; i < 5000; i++) {
    BigInt a = BigInt::random(m);
    BigInt b = BigInt::random(m);
    auto a_ring = ring.from_int(a);
    auto b_ring = ring.from_int(b);
    auto c_ring = ring.mul(a_ring, b_ring);
    BigInt c = ring.to_int(c_ring);
    EXPECT_EQ(c, a * b % m);
  }
}

template <int bits>
static void test_fixed_window_exponentiation() {
  BigInt m = generate_random_prime(bits);
  Ring<bits> ring(m);
  BigInt a = BigInt::random(m);
  BigInt e = BigInt::random(m);
  auto a_ring = ring.from_int(a);
  auto c_ring = fixed_window_exponentiate<Ring<bits>, 4, bits>(a_ring, ExponentScalar<bits>(e), ring);
  BigInt c = ring.to_int(c_ring);
  EXPECT_EQ(c, a.pow(e, m));
}

template <int bits>
static void test_ring_multiplication_batched() {
  // Test batch_mulreduce
  BigInt m = generate_random_prime(bits);
  Ring<bits> ring(m);
  BigInt a1 = BigInt::random(m);
  BigInt a2 = BigInt::random(m);
  BigInt b1 = BigInt::random(m);
  BigInt b2 = BigInt::random(m);
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  auto b1_ring = ring.from_int(b1);
  auto b2_ring = ring.from_int(b2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<typename Ring<bits>::RingElement, 2> b_rings = {b1_ring, b2_ring};
  auto c_rings = ring.template batch_mulreduce<2>(a_rings, b_rings);
  BigInt c1 = ring.to_int(c_rings[0]);
  BigInt c2 = ring.to_int(c_rings[1]);
  EXPECT_EQ(c1, a1 * b1 % m);
  EXPECT_EQ(c2, a2 * b2 % m);
}

template <int bits>
static void test_different_ring_multiplication_batched() {
  // Test batch_mulreduce
  BigInt m1 = generate_random_prime(bits);
  BigInt m2 = generate_random_prime(bits);
  Ring<bits> ring1(m1);
  Ring<bits> ring2(m2);
  BigInt a1 = BigInt::random(m1);
  BigInt a2 = BigInt::random(m2);
  BigInt b1 = BigInt::random(m1);
  BigInt b2 = BigInt::random(m2);
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring2.from_int(a2);
  auto b1_ring = ring1.from_int(b1);
  auto b2_ring = ring2.from_int(b2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<typename Ring<bits>::RingElement, 2> b_rings = {b1_ring, b2_ring};
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  auto c_rings = Ring<bits>::template true_batch_mulreduce<2>(rings, a_rings, b_rings);
  BigInt c1 = ring1.to_int(c_rings[0]);
  BigInt c2 = ring2.to_int(c_rings[1]);
  EXPECT_EQ(c1, a1 * b1 % m1);
  EXPECT_EQ(c2, a2 * b2 % m2);
}

template <int bits>
static void test_fixed_window_exponentiation_batched() {
  BigInt m = generate_random_prime(bits);
  Ring<bits> ring(m);
  BigInt a1 = BigInt::random(m);
  BigInt a2 = BigInt::random(m);
  BigInt e1 = BigInt::random(m);
  BigInt e2 = BigInt::random(m);
  auto a1_ring = ring.from_int(a1);
  auto a2_ring = ring.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<ExponentScalar<bits>, 2> e_scalars = {ExponentScalar<bits>(e1), ExponentScalar<bits>(e2)};
  auto c_rings = fixed_window_exponentiate_batched<Ring<bits>, 4, bits, 2>(a_rings, e_scalars, ring);
  BigInt c1 = ring.to_int(c_rings[0]);
  BigInt c2 = ring.to_int(c_rings[1]);
  EXPECT_EQ(c1, a1.pow(e1, m));
  EXPECT_EQ(c2, a2.pow(e2, m));
}
template <int bits, bool serialize_reduce = false>
static void test_fixed_window_exponentiation_batched_diffring() {
  BigInt m1 = generate_random_prime(bits);
  BigInt m2 = generate_random_prime(bits);
  Ring<bits> ring1(m1);
  Ring<bits> ring2(m2);
  std::array<const Ring<bits> *, 2> rings = {&ring1, &ring2};
  BigInt a1 = BigInt::random(m1);
  BigInt a2 = BigInt::random(m2);
  BigInt e1 = BigInt::random(m1);
  BigInt e2 = BigInt::random(m2);
  auto a1_ring = ring1.from_int(a1);
  auto a2_ring = ring2.from_int(a2);
  std::array<typename Ring<bits>::RingElement, 2> a_rings = {a1_ring, a2_ring};
  std::array<ExponentScalar<bits>, 2> e_scalars = {ExponentScalar<bits>(e1), ExponentScalar<bits>(e2)};
  auto c_rings = fixed_window_exponentiate_batched_diffring<Ring<bits>, 4, bits, 2, true, serialize_reduce>(a_rings, e_scalars, rings);
  BigInt c1 = ring1.to_int(c_rings[0]);
  BigInt c2 = ring2.to_int(c_rings[1]);
  EXPECT_EQ(c1, a1.pow(e1, m1));
  EXPECT_EQ(c2, a2.pow(e2, m2));
}

constexpr int avx_ifma_word_length = 52;

// Inefficient but correct implementation of word_reinterpret and int_reconstruct
// Helper function to reinterpret a BigInt as words of specified bit length
template<int limbs, int word_length>
inline void word_reinterpret_gold(const BigInt &integer, std::array<uint64_t, limbs> &digits) {
    BigInt mask = (BigInt(1) << word_length) - 1;
    for (int i = 0; i < limbs; i++) {
        BigInt shifted = integer >> (word_length * i);
        digits[i] = static_cast<uint64_t>((shifted & mask).to_ulong());
    }
}

// Helper function to reconstruct a BigInt from words of specified bit length
// Could probably be done more efficiently with bit operations.
template<int limbs, int in_limbs, int word_length>
inline BigInt int_reconstruct_gold(const std::array<uint64_t, in_limbs> &digits) {
    static_assert(limbs <= in_limbs, "limbs must be less than or equal to in_limbs");
    BigInt result(0);
    for (int i = 0; i < limbs; i++) {
        result += BigInt(digits[i]) << (word_length * i);
    }
    return result;
}

template <int bits>
static void test_word_reinterpret() {
  constexpr int limbs = ceil_div(bits, avx_ifma_word_length);
  BigInt integer = BigInt::random(BigInt(1) << bits);
  std::array<uint64_t, limbs> digits_expected;
  word_reinterpret_gold<limbs, avx_ifma_word_length>(integer, digits_expected);
  std::array<uint64_t, limbs> digits_actual;
  word_reinterpret52<limbs>(integer, digits_actual);
  // bn_to_52le(integer.get_value(), digits_actual.data(), limbs);
  for (int i = 0; i < limbs; i++) {
    EXPECT_EQ(digits_expected[i], digits_actual[i]);
  }
}

// n_out = limbs[0] + (limbs[1] << 52) + (limbs[2] << 104) + ...
inline void reconstruct_from_ovf_52bit_limbs_gold(BigInt &n_out, const uint64_t *limbs, size_t num_limbs) {
  // Upper 12 bits of limbs[i] need not be zero. This will be incorporated correctly.
  size_t n_data_size_u64 = ((num_limbs + 1) / 2) * 13 / 8 + 1; // Number of 64-bit limbs
  size_t n_data_size = n_data_size_u64 * 8;
  uint8_t *n_data_even = new uint8_t[n_data_size]; // The 1st, 3rd, 5th... limbs (even in 0-based indexing)
  uint8_t *n_data_odd = new uint8_t[n_data_size]; // The 2nd, 4th, 6th... limbs
  memset(n_data_even, 0, n_data_size);
  memset(n_data_odd, 0, n_data_size);
  for (size_t i = 0; i < num_limbs; i += 2) {
      memcpy(n_data_even + (i / 2) * 13, limbs + i, 8);
  }
  for (size_t i = 1; i < num_limbs; i += 2) {
      memcpy(n_data_odd + (i / 2) * 13, limbs + i, 8);
  }
  fast_import_bigint(n_out, n_data_even, n_data_size_u64);
  BigInt tmp;
  fast_import_bigint(tmp, n_data_odd, n_data_size_u64);
  tmp <<= 52; // TODO: make this constant-time, or find a memcpy hack to avoid a shift like word_reinterpret52
  // n_out += tmp; // non constant-time code
  bn_uadd_consttime(n_out.get_value(), n_out.get_value(), tmp.get_value());
  delete[] n_data_odd;
  delete[] n_data_even;
}

template <int bits>
static void test_reconstruct_from_ovf_52bit_limbs() {
  constexpr int limbs = ceil_div(bits, avx_ifma_word_length);
  std::array<uint64_t, limbs> limbs_in;
  for (int i = 0; i < limbs; i++) {
    limbs_in[i] = BigInt::random(BigInt(1) << 60).to_ulong();
  }
  BigInt n_out;
  reconstruct_from_ovf_52bit_limbs_gold(n_out, limbs_in.data(), limbs);
  BigInt n_out_actual;
  reconstruct_from_ovf_52bit_limbs<limbs>(n_out_actual, limbs_in);
  EXPECT_EQ(n_out, n_out_actual);
}

TEST(RNSTest, ToInt52) {
  test_to_int<52>();
}

TEST(RNSTest, ToInt63) {
  test_to_int<63>();
}

TEST(RNSTest, ToInt64) {
  test_to_int<64>();
}

TEST(RNSTest, ToInt65) {
  test_to_int<65>();
}

TEST(RNSTest, ToInt103) {
  test_to_int<103>();
}

TEST(RNSTest, ToInt104) {
  test_to_int<104>();
}

TEST(RNSTest, ToInt105) {
  test_to_int<105>();
}

TEST(RNSTest, ToInt127) {
  test_to_int<127>();
}

TEST(RNSTest, ToInt128) {
  test_to_int<128>();
}

TEST(RNSTest, ToInt129) {
  test_to_int<129>();
}

TEST(RNSTest, ToInt192) {
  test_to_int<192>();
}

TEST(RNSTest, ToInt1024) {
  test_to_int<1024>();
}

TEST(RNSTest, ToInt1536) {
  test_to_int<1536>();
}

TEST(RNSTest, ToInt2048) {
  test_to_int<2048>();
}

TEST(RNSTest, RingMultiplication64) {
  test_ring_multiplication<64>();
}

TEST(RNSTest, RingMultiplication128) {
  test_ring_multiplication<128>();
}

TEST(RNSTest, RingMultiplication192) {
  test_ring_multiplication<192>();
}

TEST(RNSTest, RingMultiplication1024) {
  test_ring_multiplication<1024>();
}

TEST(RNSTest, RingMultiplication1536) {
  test_ring_multiplication<1536>();
}

TEST(RNSTest, RingMultiplication2048) {
  test_ring_multiplication<2048>();
}

TEST(RNSTest, FixedWindowExponentiation1024) {
  test_fixed_window_exponentiation<1024>();
}

TEST(RNSTest, FixedWindowExponentiation1536) {
  test_fixed_window_exponentiation<1536>();
}

TEST(RNSTest, FixedWindowExponentiation2048) {
  test_fixed_window_exponentiation<2048>();
}

// Batching tests below:

TEST(RNSTest, RingMultiplicationBatched1024) {
  test_ring_multiplication_batched<1024>();
}

TEST(RNSTest, RingMultiplicationBatched1536) {
  test_ring_multiplication_batched<1536>();
}

TEST(RNSTest, RingMultiplicationBatched2048) {
  test_ring_multiplication_batched<2048>();
}

TEST(RNSTest, DifferentRingMultiplicationBatched1024) {
  test_different_ring_multiplication_batched<1024>();
}

TEST(RNSTest, DifferentRingMultiplicationBatched1536) {
  test_different_ring_multiplication_batched<1536>();
}

TEST(RNSTest, DifferentRingMultiplicationBatched2048) {
  test_different_ring_multiplication_batched<2048>();
}

TEST(RNSTest, FixedWindowExponentiationBatched1024) {
  test_fixed_window_exponentiation_batched<1024>();
}

TEST(RNSTest, FixedWindowExponentiationBatched1536) {
  test_fixed_window_exponentiation_batched<1536>();
}

TEST(RNSTest, FixedWindowExponentiationBatched2048) {
  test_fixed_window_exponentiation_batched<2048>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring64) {
  test_fixed_window_exponentiation_batched_diffring<64>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring128) {
  test_fixed_window_exponentiation_batched_diffring<128>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1024) {
  test_fixed_window_exponentiation_batched_diffring<1024>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1024SerializeReduce) {
  test_fixed_window_exponentiation_batched_diffring<1024, true>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1536) {
  test_fixed_window_exponentiation_batched_diffring<1536>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1536SerializeReduce) {
  test_fixed_window_exponentiation_batched_diffring<1536, true>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring2048) {
  test_fixed_window_exponentiation_batched_diffring<2048>();
}

TEST(RNSTest, FixedWindowExponentiationBatchedDiffring2048SerializeReduce) {
  test_fixed_window_exponentiation_batched_diffring<2048, true>();
}

TEST(RNSTest, WordReinterpret64) {
  test_word_reinterpret<64>();
}

TEST(RNSTest, WordReinterpret128) {
  test_word_reinterpret<128>();
}

TEST(RNSTest, WordReinterpret192) {
  test_word_reinterpret<192>();
}

TEST(RNSTest, WordReinterpret1024) {
  test_word_reinterpret<1024>();
}

TEST(RNSTest, WordReinterpret1536) {
  test_word_reinterpret<1536>();
}

TEST(RNSTest, WordReinterpret2048) {
  test_word_reinterpret<2048>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs64) {
  test_reconstruct_from_ovf_52bit_limbs<64>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs128) {
  test_reconstruct_from_ovf_52bit_limbs<128>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs192) {
  test_reconstruct_from_ovf_52bit_limbs<192>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs1024) {
  test_reconstruct_from_ovf_52bit_limbs<1024>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs1536) {
  test_reconstruct_from_ovf_52bit_limbs<1536>();
}

TEST(RNSTest, ReconstructFromOvf52BitLimbs2048) {
  test_reconstruct_from_ovf_52bit_limbs<2048>();
}
