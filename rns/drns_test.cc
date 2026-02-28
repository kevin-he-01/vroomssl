#include <gtest/gtest.h>
#include "../rns/crypto/exponentiation.hpp"
#include "../rns/crypto/drns_ring.hpp"
#include <openssl/bn.h>

#define Ring DRNSRing

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

template <int bits, typename Variant>
static void test_ring_multiplication() {
  BigInt m = generate_random_prime(bits);
  Ring<bits, 0> ring(m);
  BigInt a = BigInt::random(m);
  BigInt b = BigInt::random(m);
  auto a_ring = ring.from_int(a);
  auto b_ring = ring.from_int(b);
  auto c_ring = ring.template mul<Variant>(a_ring, b_ring);
  BigInt c = ring.to_int(c_ring);
  EXPECT_EQ(c, a * b % m);
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

template <int bits>
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
  auto c_rings = fixed_window_exponentiate_batched_diffring<Ring<bits>, 4, bits, 2, true, false>(a_rings, e_scalars, rings);
  BigInt c1 = ring1.to_int(c_rings[0]);
  BigInt c2 = ring2.to_int(c_rings[1]);
  EXPECT_EQ(c1, a1.pow(e1, m1));
  EXPECT_EQ(c2, a2.pow(e2, m2));
}

TEST(DRNSTest, RingMultiplication1024Plain) {
  test_ring_multiplication<1024, DRNSVariant<false, false>>();
}

TEST(DRNSTest, RingMultiplication1536Plain) {
  test_ring_multiplication<1536, DRNSVariant<false, false>>();
}

TEST(DRNSTest, RingMultiplication2048Plain) {
  test_ring_multiplication<2048, DRNSVariant<false, false>>();
}

TEST(DRNSTest, RingMultiplication1024BatEmult) {
  test_ring_multiplication<1024, DRNSVariant<true, false>>();
}

TEST(DRNSTest, RingMultiplication1536BatEmult) {
  test_ring_multiplication<1536, DRNSVariant<true, false>>();
}

TEST(DRNSTest, RingMultiplication2048BatEmult) {
  test_ring_multiplication<2048, DRNSVariant<true, false>>();
}

TEST(DRNSTest, RingMultiplication1024SumProd) {
  test_ring_multiplication<1024, DRNSVariant<false, true>>();
}

TEST(DRNSTest, RingMultiplication1536SumProd) {
  test_ring_multiplication<1536, DRNSVariant<false, true>>();
}

TEST(DRNSTest, RingMultiplication2048SumProd) {
  test_ring_multiplication<2048, DRNSVariant<false, true>>();
}

TEST(DRNSTest, FixedWindowExponentiation1024) {
  test_fixed_window_exponentiation<1024>();
}

TEST(DRNSTest, FixedWindowExponentiation1536) {
  test_fixed_window_exponentiation<1536>();
}

TEST(DRNSTest, FixedWindowExponentiation2048) {
  test_fixed_window_exponentiation<2048>();
}

// Batching tests below:

// TEST(DRNSTest, RingMultiplicationBatched1024) {
//   test_ring_multiplication_batched<1024>();
// }

// TEST(DRNSTest, RingMultiplicationBatched1536) {
//   test_ring_multiplication_batched<1536>();
// }

// TEST(DRNSTest, RingMultiplicationBatched2048) {
//   test_ring_multiplication_batched<2048>();
// }

TEST(DRNSTest, DifferentRingMultiplicationBatched1024) {
  test_different_ring_multiplication_batched<1024>();
}

TEST(DRNSTest, DifferentRingMultiplicationBatched1536) {
  test_different_ring_multiplication_batched<1536>();
}

TEST(DRNSTest, DifferentRingMultiplicationBatched2048) {
  test_different_ring_multiplication_batched<2048>();
}

// TEST(DRNSTest, FixedWindowExponentiationBatched1024) {
//   test_fixed_window_exponentiation_batched<1024>();
// }

// TEST(DRNSTest, FixedWindowExponentiationBatched1536) {
//   test_fixed_window_exponentiation_batched<1536>();
// }

// TEST(DRNSTest, FixedWindowExponentiationBatched2048) {
//   test_fixed_window_exponentiation_batched<2048>();
// }

// TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1024) {
//   test_fixed_window_exponentiation_batched_diffring<1024>();
// }

// TEST(RNSTest, FixedWindowExponentiationBatchedDiffring1536) {
//   test_fixed_window_exponentiation_batched_diffring<1536>();
// }

// TEST(RNSTest, FixedWindowExponentiationBatchedDiffring2048) {
//   test_fixed_window_exponentiation_batched_diffring<2048>();
// }
