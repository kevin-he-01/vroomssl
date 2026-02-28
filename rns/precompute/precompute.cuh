#include "bigint_cuda.cuh"
#include <cuda/std/array>

// Precompute should be a separate kernel to make it easy to measure the performance without the precompute overhead
// Therefore, having multiple blocks (of 32 threads) compute the different values is okay.

// We will have each compute based on blockIdx.x

template<int bits, size_t LIMBS_IN, size_t LIMBS_OUT, size_t INDIGITS, int INBITS, size_t OUTDIGITS, int OUTBITS, int PRECISION>
void precompute(
    const cuda::std::array<BigNum<bits>, LIMBS_IN> &moduli_in,
    const cuda::std::array<BigNum<bits>, LIMBS_OUT> &moduli_out,
    const BigNum<bits> &target_modulus,
    const BigNum<bits> &premult,
    const BigNum<bits> &postmult,
    cuda::std::array<std::array<uint32_t, LIMBS_OUT * OUTDIGITS>, LIMBS_IN * INDIGITS> &rns_values_tf,
    std::array<uint64_t, LIMBS_OUT * OUTDIGITS> &correction,
    std::array<BigNum<bits>, LIMBS_IN * INDIGITS> &shifted_quotient_estimations_tf
) {

}