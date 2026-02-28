#include <benchmark/benchmark.h>
#include "../rns/vector/multiplication_common.hpp"

constexpr int w = 52;

template <int bits>
static void BM_IRNSEleMult10(benchmark::State &state) {
  constexpr int limbs = ceil_div(bits, w);
  std::array<uint64_t, limbs> moduli;
  for (int i = 0; i < limbs; i++) {
    moduli[i] = BigInt::random(BigInt(1) << w).to_ulong();
    // Ensure moduli is odd
    moduli[i] |= 1;
  }
  MontgomeryReduce<limbs> r(moduli);
  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> a_data;
  a_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      a_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> a(0);
  a.load(a_data.data());
  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> b_data;
  b_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      b_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> b(0);
  b.load(b_data.data());
  for (auto _ : state) {
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    a = ele_mult(a, b, r);
    benchmark::DoNotOptimize(a);
  }
}

template <int bits>
static void BM_IRNSEleSumProd10(benchmark::State &state) {
  constexpr int limbs = ceil_div(bits, w);
  std::array<uint64_t, limbs> moduli;
  for (int i = 0; i < limbs; i++) {
    moduli[i] = BigInt::random(BigInt(1) << w).to_ulong();
    // Ensure moduli is odd
    moduli[i] |= 1;
  }
  MontgomeryReduce<limbs> r(moduli);

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> a_data;
  a_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      a_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> a(0);
  a.load(a_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> b_data;
  b_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      b_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> b(0);
  b.load(b_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> c_data;
  c_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      c_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> c(0);
  c.load(c_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> d_data;
  d_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      d_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> d(0);
  d.load(d_data.data());

  for (auto _ : state) {
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    a = ele_sum_prod(a, b, c, d, r);
    benchmark::DoNotOptimize(a);
  }
}

template <int bits>
static void BM_IRNSEleMult10Batch2(benchmark::State &state) {
  constexpr int limbs = ceil_div(bits, w);
  std::array<uint64_t, limbs> moduli;
  for (int i = 0; i < limbs; i++) {
    moduli[i] = BigInt::random(BigInt(1) << w).to_ulong();
    // Ensure moduli is odd
    moduli[i] |= 1;
  }
  MontgomeryReduce<limbs> r(moduli);

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> a_data;
  a_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      a_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> a(0);
  a.load(a_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> b_data;
  b_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      b_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> b(0);
  b.load(b_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> c_data;
  c_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      c_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> c(0);
  c.load(c_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> d_data;
  d_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      d_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> d(0);
  d.load(d_data.data());

  for (auto _ : state) {
    // TODO: there is likely a more clever way to batch this by interleaving the instructions inside manually, or maybe there is already a function that does this?
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(c);
  }
}

template <int bits>
static void BM_IRNSEleMult10Batch2Alt(benchmark::State &state) {
  constexpr int limbs = ceil_div(bits, w);
  std::array<uint64_t, limbs> moduli;
  for (int i = 0; i < limbs; i++) {
    moduli[i] = BigInt::random(BigInt(1) << w).to_ulong();
    // Ensure moduli is odd
    moduli[i] |= 1;
  }
  MontgomeryReduce<limbs> r(moduli);

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> a_data;
  a_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      a_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> a(0);
  a.load(a_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> b_data;
  b_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      b_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> b(0);
  b.load(b_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> c_data;
  c_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      c_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> c(0);
  c.load(c_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> d_data;
  d_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      d_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> d(0);
  d.load(d_data.data());

  std::array<AVXVector<limbs>, 2> ac{a, c};
  std::array<AVXVector<limbs>, 2> bd{b, d};
  std::array<MontgomeryReduce<limbs>, 2> rr{r, r};

  for (auto _ : state) {
    {
      auto [a_new, c_new] = batch_ele_mult(ac, bd, rr);
      ac[0] = a_new;
      ac[1] = c_new;
    }
    {
      auto [a_new, c_new] = batch_ele_mult(ac, bd, rr);
      ac[0] = a_new;
      ac[1] = c_new;
    }
    {
      auto [a_new, c_new] = batch_ele_mult(ac, bd, rr);
      ac[0] = a_new;
      ac[1] = c_new;
    }
    {
      auto [a_new, c_new] = batch_ele_mult(ac, bd, rr);
      ac[0] = a_new;
      ac[1] = c_new;
    }
    {
      auto [a_new, c_new] = batch_ele_mult(ac, bd, rr);
      ac[0] = a_new;
      ac[1] = c_new;
    }
    benchmark::DoNotOptimize(ac[0]);
    benchmark::DoNotOptimize(ac[1]);
  }
}

template <int bits>
static void BM_IRNSEleMult20Batch4(benchmark::State &state) {
  constexpr int limbs = ceil_div(bits, w);
  std::array<uint64_t, limbs> moduli;
  for (int i = 0; i < limbs; i++) {
    moduli[i] = BigInt::random(BigInt(1) << w).to_ulong();
    // Ensure moduli is odd
    moduli[i] |= 1;
  }
  MontgomeryReduce<limbs> r(moduli);

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> a_data;
  a_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      a_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> a(0);
  a.load(a_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> b_data;
  b_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      b_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> b(0);
  b.load(b_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> c_data;
  c_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      c_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> c(0);
  c.load(c_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> d_data;
  d_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      d_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> d(0);
  d.load(d_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> e_data;
  e_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      e_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> e(0);
  e.load(e_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> f_data;
  f_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      f_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> f(0);
  f.load(f_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> g_data;
  g_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      g_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> g(0);
  g.load(g_data.data());

  std::array<uint64_t, AVXVector<limbs>::VEC_LIMBS * AVXVector<limbs>::LIMBS_PER_VEC> h_data;
  h_data.fill(0); // Fill with zeros to prevent uninitialized values
  for (int i = 0; i < limbs; i++) {
      h_data[i] = static_cast<uint64_t>((BigInt::random(moduli[i])).to_ulong());
  }
  AVXVector<limbs> h(0);
  h.load(h_data.data());

  for (auto _ : state) {
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    e = ele_mult(e, f, r);
    g = ele_mult(g, h, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    e = ele_mult(e, f, r);
    g = ele_mult(g, h, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    e = ele_mult(e, f, r);
    g = ele_mult(g, h, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    e = ele_mult(e, f, r);
    g = ele_mult(g, h, r);
    a = ele_mult(a, b, r);
    c = ele_mult(c, d, r);
    e = ele_mult(e, f, r);
    g = ele_mult(g, h, r);
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(c);
  }
}

// TODO: benchmark rns_reduce_with_acc and rns_reduce, as well as the batched variants

BENCHMARK_TEMPLATE(BM_IRNSEleMult10, 1024); // 2048-bit RSA, 1024-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10, 1536); // 3072-bit RSA, 1536-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10, 2048); // 4096-bit RSA, 2048-bit prime

BENCHMARK_TEMPLATE(BM_IRNSEleSumProd10, 1024); // 2048-bit RSA, 1024-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleSumProd10, 1536); // 3072-bit RSA, 1536-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleSumProd10, 2048); // 4096-bit RSA, 2048-bit prime

BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2, 1024); // 2048-bit RSA, 1024-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2, 1536); // 3072-bit RSA, 1536-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2, 2048); // 4096-bit RSA, 2048-bit prime

BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2Alt, 1024); // 2048-bit RSA, 1024-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2Alt, 1536); // 3072-bit RSA, 1536-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult10Batch2Alt, 2048); // 4096-bit RSA, 2048-bit prime

BENCHMARK_TEMPLATE(BM_IRNSEleMult20Batch4, 1024); // 2048-bit RSA, 1024-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult20Batch4, 1536); // 3072-bit RSA, 1536-bit prime
BENCHMARK_TEMPLATE(BM_IRNSEleMult20Batch4, 2048); // 4096-bit RSA, 2048-bit prime
