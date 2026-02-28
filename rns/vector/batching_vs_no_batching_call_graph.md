# Call graphs

- Ring in `ring.hpp`
- RNSMatrix in `changebase.hpp`
- IntRNS2 in `multiplication.hpp`
- MontgomeryReduce in `montgomery.hpp`

## No batching

- `IntRNS2::mul_reduce`:
  - `ele_mult`
    - `MontgomeryReduce::reduce_small(this = r = m1)` # mod M
  - `RNSMatrix::rns_reduce_with_acc(this = r1)` # 1st CRNS matrix
    - `RNSMatrix::rns_reduce_raw`
    - `MontgomeryReduce::reduce_full` # mod N
  - `RNSMatrix::rns_reduce(this = r2)` # 2nd CRNS matrix
    - `RNSMatrix::rns_reduce_raw`
    - `MontgomeryReduce::reduce_full` # mod M

## Batching

- `Ring::batch_mulreduce`
  - `Ring::wide_mul`
  - `Ring::batch_wide_reduce_expand`
    - `IntRNS2::reduce_wide_batch`
      - `MontgomeryReduce::reduce_small_batch(this = m1)` # mod M
      - `RNSMatrix::rns_reduce_raw_batch(this = r1)` # 1st CRNS matrix
      - `MontgomeryReduce::reduce_full_batch(this = m2)` # mod N
    - `IntRNS2::batch_expand_wide`
      - `RNSMatrix::rns_reduce_raw_batch(this = r2)` # 2nd CRNS matrix
      - `RNSMatrix::reduce_full_batch(this = m1)` # mod M

## Unrolled algorithm (`Ring::batch_mulreduce`)

Definitions:
- `RingElement` = $t \times t$
- `RingElementWide` = $2t \times 2t$
- `Ring::wide_mul(RingElement, RingElement) -> RingElementWide` = $(a_i, a_j) \times (b_i, b_j) \mapsto (a_i * b_i, a_j * b_j)$

Given:
- `const std::array<RingElement, batch> &a` = $(a_i', a_j')$
- `const std::array<RingElement, batch> &b` = $(b_i', b_j')$

Computes:
- $s'_i: 2t = a_i' * b_i', \;\; s'_j: 2t = a_j' * b_j'$
- `result_wide` = $(s'_i, s'_j) \in [0, M^2] \times [0, N^2]$
  ```cpp
  for (int i = 0; i < batch; i++) { // In Ring::batch_mulreduce
    result_wide[i] = wide_mul(a[i], b[i]);
  }
  ```
- `Ring::batch_wide_reduce_expand(result_wide)`
  - `[a1_lo, a1_hi]` = $s_i': 2t \in [0, M^2]$
  - `[a2_lo, a2_hi]` = $s_j': 2t \in [0, N^2]$
  - `result_m2 = IntRNS2::reduce_wide_batch(a1_lo, a1_hi, a2_lo, a2_hi)`
    - `a1 = MontgomeryReduce::reduce_small_batch(this = m1, a1_hi, a1_lo)`
      i.e., $s_i: t = s_i' \mod M $
    - `[a2_lo_copy, a2_hi_copy] = [a2_lo, a2_hi]`
    - `[a2_lo_copy, a2_hi_copy] += RNSMatrix::rns_reduce_raw_batch(this = r1, a1)`, i.e., 1st CRNS matrix
      - $q_j': (2t + \epsilon) = CRNS^{M * d * 2^{-w}}_{N * p * M^{-2} * 2^{2w}} (s_i) $
      - $r_j': (2t + \epsilon) = s_j' + q_j' = a_j' * b_j' + q_j'$
    - `return result_m2 = out2 = MontgomeryReduce::reduce_full_batch(this = m2, a2_hi_copy, a2_lo_copy)`
      i.e., $r_j: t = r_j' \mod N$
  - `result_m1 = IntRNS2::batch_expand_wide(result_m2)`
    - `[z_lo, z_hi] = RNSMatrix::rns_reduce_raw_batch(this = r2, result_m2)`
      i.e., $r_i': (2t + \epsilon) = CRNS^{N * M * 2^{-w}}_{M * 2^{2w}} (r_j) $
    - `out1 = MontgomeryReduce::reduce_full_batch([z_lo, z_hi])`
      i.e., $r_i: t = r_i' \mod M $

## Misc (call graph extensions)

- RNSMatrix::rns_reduce_raw
  - RNSMatrix::accumulate_loop
  - RNSMatrix::add_correction

## Opt Mont algorithm

M, N are t limbs long (t x 52-bit moduli)

- $CRNS: t \to (2t + \epsilon)$  
  _Remark:_ This definition of CRNS does NOT reduce the output from $2t$ down to $t$ to allow for possible downstream optimizations. Since CRNS has multiplicative depth 1, it outputs a number of size $2t$ instead of $t$
- $*: t \times t \to 2t$
- `reduce_small`: $\cdot \mod M: 2t \to t$
- `reduce_full`: $\cdot \mod M: (2t + \epsilon) \to t$

Given:
- $ a_i': t $
- $ a_j': t $
- $ b_i': t $
- $ b_j': t $

Compute:
- $ s_i': 2t = a_i' * b_i' $
- $s_i: t = s_i' \mod M $ (`reduce_small`)
- $q_j': (2t + \epsilon) = CRNS^{M * d * 2^{-w}}_{N * p * M^{-2} * 2^{2w}} (s_i) $
- $r_j': (2t + \epsilon) = a_j' * b_j' + q_j' $ (Note $s_j': 2t = a_j' * b_j'$ is implicit here.)
- $r_j: t = r_j' \mod N $ (`reduce_full`)
- $r_i': (2t + \epsilon) = CRNS^{N * M * 2^{-w}}_{M * 2^{2w}} (r_j) $
- $r_i: t = r_i' \mod M $ (`reduce_full`)

Output $(r_i, r_j)$
