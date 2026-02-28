#pragma once
#include "ring.hpp"


template <int modbits>
class ExponentScalar {
    static constexpr int limbs = ceil_div(modbits, 64);
    uint64_t data[limbs];

public:
    inline ExponentScalar(const BigInt &i) {
        // for (int j = 0; j < limbs; j++) {
        //     data[j] = (i >> (j * 64)).to_ulong();
        // }
        // Intel is a little endian machine, so reinterpret cast works.
        if (!BN_bn2le_padded(reinterpret_cast<uint8_t*>(data), limbs * sizeof(uint64_t), i.get_value())) {
            ERR_print_errors_fp(stderr);
            fprintf(stderr, "BN_bn2le_padded failed\n");
            abort();
        }
    }

    INLINE
    inline int get_bit(int bit_pos) const {
        return (data[bit_pos / 64] >> (bit_pos % 64)) & 1;
    }

    INLINE
    inline uint64_t get_bit_same_word(int start_bit, int end_bit_exclusive) const {
        int start_word = start_bit / 64;
        return (data[start_word] >> (start_bit % 64)) & ((1ULL << (end_bit_exclusive - start_bit)) - 1ULL);
    }

    INLINE
    inline int get_bits(int start_bit, int end_bit) const {
        int end_bit_exclusive = end_bit + 1;
        int end_word = end_bit_exclusive / 64;
        int start_word = start_bit / 64;
        if (end_word == start_word) {
            return static_cast<int>(get_bit_same_word(start_bit, end_bit_exclusive));
        } else {
            assert(end_word == start_word + 1); // window size <= 64 for exponentiation
            return static_cast<int>(get_bit_same_word(start_bit, end_word * 64) | (get_bit_same_word(end_word * 64, end_bit_exclusive) << (end_word * 64 - start_bit)));
        }
        // {
        //     // General routine
        //     int result = 0;
        //     int bit_count = 0;
        //     constexpr int max_bits = modbits;

        //     // Handle out of bounds: clamp to valid range
        //     if (start_bit < 0) start_bit = 0;
        //     if (end_bit >= max_bits) end_bit = max_bits - 1;
        //     if (start_bit > end_bit) return 0;

        //     // Extract bits from start_bit to end_bit (inclusive)
        //     for (int i = start_bit; i <= end_bit; i++) {
        //         int bit = get_bit(i);
        //         result |= (bit << bit_count);
        //         bit_count++;
        //     }
        //     return result;
        // }
    }
};

// Left-to-right exponentiation using constant-time selection
// Always processes scalar_bits iterations for constant-time execution (prevents timing side channels)
template<class Ring, int scalar_bits>
typename Ring::RingElement exponentiate(const typename Ring::RingElement &base, const ExponentScalar<scalar_bits> &exponent, const Ring &ring) {
    // Pre-compute one element to avoid repeated calls
    typename Ring::RingElement one_elem = ring.one();

    // could return 1 here if scalar_bits == 0 (template would still be constant time), but we'll assert that it's positive
    static_assert(scalar_bits > 0, "scalar_bits must positive");
    typename Ring::RingElement result = ring.ctime_select(exponent.get_bit(scalar_bits - 1), base, one_elem);
    
    // Process all bits from MSB to LSB for constant-time execution
    for (int i = scalar_bits - 2; i >= 0; i--) {
        result = ring.square(result);
        
        // Use constant-time select to avoid timing side channels
        int bit_val = exponent.get_bit(i);
        bool bit_set = (bit_val != 0);
        typename Ring::RingElement op = ring.ctime_select(bit_set, base, one_elem);
        result = ring.mul(result, op);
    }
    
    return result;
}

template<class Ring, int window_bits>
class FixedWindowTable {
    // const Ring& ring_ptr;
    public:
    std::array<typename Ring::RingElement, 1<<window_bits> window_table;
    FixedWindowTable() {
        // Empty constructor for batched version
    }
    INLINE
    FixedWindowTable(const typename Ring::RingElement &base, const Ring &ring) {
        window_table[0] = ring.one();
        window_table[1] = base;
        for (int i = 2; i < (1<<window_bits); i++) {
            // if (i % 2 == 0) {
            //     window_table[i] = ring.square(window_table[i/2]);
            // } else {
            //     window_table[i] = ring.mul(window_table[i-1], base);
            // }
            window_table[i] = ring.mul(window_table[i-1], base);
        }
    }

    // If you don't care about constant time, you can just use the array directly
    inline typename Ring::RingElement operator[](int index) const {
        // return window_table[index];
        fprintf(stderr, "Not implemented: operator[]\n");
        abort();
    }

    // ctime select through linear scan
    INLINE
    inline typename Ring::RingElement ctime_select(int index) const {
        typename Ring::RingElement result = window_table[0];
        for (int i = 0; i < (1<<window_bits); i++) {
            result = Ring::ctime_select_static(i == index, window_table[i], result);
        }
        return result;
    }
};

// Left-to-right exponentiation using constant-time selection
// Always processes scalar_bits iterations for constant-time execution (prevents timing side channels)
template<class Ring, int window_bits, int scalar_bits, bool constant_time = true>
typename Ring::RingElement fixed_window_exponentiate(const typename Ring::RingElement &base, const ExponentScalar<scalar_bits> &exponent, const Ring &ring) {
    // Create window table
    FixedWindowTable<Ring, window_bits> window_table(base, ring);
    
    static_assert(window_bits <= scalar_bits, "window_bits must be <= scalar_bits");
    
    int rem_bits = scalar_bits % window_bits;
    
    // Start with the most significant chunk
    int first_chunk_start = scalar_bits - window_bits;
    int first_chunk_bits = exponent.get_bits(first_chunk_start, scalar_bits - 1);
    typename Ring::RingElement result = window_table.ctime_select(first_chunk_bits);
    
    // Process full chunks from MSB to LSB (skip remainder chunk)
    int last_full_chunk_start = (rem_bits > 0) ? rem_bits : 0;
    for (int current_bit = scalar_bits - 2 * window_bits; current_bit >= last_full_chunk_start; current_bit -= window_bits) {
        // Square window_bits times
        for (int j = 0; j < window_bits; j++) {
            result = ring.square(result);
        }
        // Get bits for this chunk
        int chunk_end = current_bit + window_bits - 1;
        int bits = exponent.get_bits(current_bit, chunk_end);
        // Select from window table and multiply
        typename Ring::RingElement op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(bits);
        } else {
            op = window_table[bits];
        }
        result = ring.mul(result, op);
    }
    
    // Handle remainder chunk (if any) - get_bits handles out-of-bounds gracefully
    if (rem_bits > 0) {
        // Square rem_bits times (not window_bits times)
        for (int j = 0; j < rem_bits; j++) {
            result = ring.square(result);
        }
        // Get the remainder bits (get_bits handles bounds)
        int remainder_bits = exponent.get_bits(0, rem_bits - 1);
        typename Ring::RingElement op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(remainder_bits);
        } else {
            op = window_table[remainder_bits];
        }
        result = ring.mul(result, op);
    }
    
    return result;
}

template<class Ring, int window_bits, int batch_size>
class FixedWindowTableBatched {
    using BatchElement = std::array<typename Ring::RingElement, batch_size>;
    std::array<BatchElement, 1<<window_bits> window_table;
    const Ring& ring_ptr;
    public:
    FixedWindowTableBatched(const BatchElement &base, const Ring &ring) : ring_ptr(ring) {
        window_table[0] = BatchElement{ring.one(), ring.one()};
        window_table[1] = base;
        for (int i = 2; i < (1<<window_bits); i++) {
            if (i % 2 == 0) {
                window_table[i] = ring.template batch_mulreduce<batch_size>(window_table[i/2], window_table[i/2]);
            } else {
                window_table[i] = ring.template batch_mulreduce<batch_size>(window_table[i-1], base);
            }
        }
    }

    // If you don't care about constant time, you can just use the array directly
    inline BatchElement operator[](int index) const {
        fprintf(stderr, "Not implemented: operator[]\n");
        abort();
        // return window_table[index];
    }

    // ctime select through linear scan
    INLINE
    inline BatchElement ctime_select(std::array<int, batch_size> indices) const {
        BatchElement result = window_table[0];
        for (int i = 0; i < (1<<window_bits); i++) {
            for (int j = 0; j < batch_size; j++) {
                result[j] = ring_ptr.ctime_select(i == indices[j], window_table[i][j], result[j]);
            }
        }
        return result;
    }
};

// template<class Ring, int window_bits, int batch_size>
// class FixedWindowTableBatchedMultiRingSlow {
//     using BatchElement = std::array<typename Ring::RingElement, batch_size>;
//     std::array<BatchElement, 1<<window_bits> window_table;
//     const std::array<const Ring *, batch_size> &rings;
//     public:
//     FixedWindowTableBatchedMultiRingSlow(const BatchElement &base, const std::array<const Ring *, batch_size> &rings) : rings(rings) {
//         for (int i = 0; i < batch_size; i++) {
//             window_table[0][i] = rings[i]->one();
//         }
//         window_table[1] = base;
//         for (int i = 2; i < (1<<window_bits); i++) {
//             if (i % 2 == 0) {
//                 window_table[i] = Ring::template true_batch_mulreduce<batch_size>(rings, window_table[i/2], window_table[i/2]);
//             } else {
//                 window_table[i] = Ring::template true_batch_mulreduce<batch_size>(rings, window_table[i-1], base);
//             }
//         }
//     }

//     // If you don't care about constant time, you can just use the array directly
//     inline BatchElement operator[](int index) const {
//         return window_table[index];
//     }

//     // ctime select through linear scan
//     INLINE
//     inline BatchElement ctime_select(std::array<int, batch_size> indices) const {
//         BatchElement result = window_table[0];
//         for (int i = 0; i < (1<<window_bits); i++) {
//             for (int j = 0; j < batch_size; j++) {
//                 result[j] = Ring::ctime_select_static(i == indices[j], window_table[i][j], result[j]);
//             }
//         }
//         return result;
//     }
// };

template<class Ring, int window_bits, int batch_size>
class FixedWindowTableBatchedMultiRing {
    using BatchElement = std::array<typename Ring::RingElement, batch_size>;
    std::array<FixedWindowTable<Ring, window_bits>, batch_size> window_tables;
    const std::array<const Ring *, batch_size> &rings;
    public:
    INLINE
    inline FixedWindowTableBatchedMultiRing(const BatchElement &base, const std::array<const Ring *, batch_size> &rings) : rings(rings) {
        for (int i = 0; i < batch_size; i++) {
            window_tables[i].window_table[0] = rings[i]->one();
            window_tables[i].window_table[1] = base[i];
        }
        for (int i = 2; i < (1<<window_bits); i++) {
            // window_table[i] = Ring::template true_batch_mulreduce<batch_size>(rings, window_table[i-1], base);
            BatchElement inputs;
            for (int j = 0; j < batch_size; j++) {
                inputs[j] = window_tables[j].window_table[i-1];
            }
            BatchElement result = Ring::template true_batch_mulreduce<batch_size>(rings, inputs, base);
            for (int j = 0; j < batch_size; j++) {
                window_tables[j].window_table[i] = result[j];
            }
        }
    }

    // If you don't care about constant time, you can just use the array directly
    inline BatchElement operator[](int index) const {
        fprintf(stderr, "Not implemented: operator[]\n");
        abort();
    }

    // ctime select through linear scan
    INLINE
    inline BatchElement ctime_select(std::array<int, batch_size> indices) const {
        BatchElement result;
        for (int j = 0; j < batch_size; j++) {
            result[j] = window_tables[j].ctime_select(indices[j]);
        }
        return result;
    }
};

// Left-to-right exponentiation using constant-time selection
// Always processes scalar_bits iterations for constant-time execution (prevents timing side channels)
template<class Ring, int window_bits, int scalar_bits, int batch_size, bool constant_time = true>
std::array<typename Ring::RingElement, batch_size> fixed_window_exponentiate_batched(const std::array<typename Ring::RingElement, batch_size> &base, const std::array<ExponentScalar<scalar_bits>, batch_size> &exponents, const Ring &ring) {
    // Create window table
    FixedWindowTableBatched<Ring, window_bits, batch_size> window_table(base, ring);
    
    static_assert(window_bits <= scalar_bits, "window_bits must be <= scalar_bits");
    
    int rem_bits = scalar_bits % window_bits;
    
    // Start with the most significant chunk
    int first_chunk_start = scalar_bits - window_bits;
    std::array<int, batch_size> first_chunk_bits;
    for (int j = 0; j < batch_size; j++) {
        first_chunk_bits[j] = exponents[j].get_bits(first_chunk_start, scalar_bits - 1);
    }
    std::array<typename Ring::RingElement, batch_size> result = window_table.ctime_select(first_chunk_bits);
    
    // Process full chunks from MSB to LSB (skip remainder chunk)
    int last_full_chunk_start = (rem_bits > 0) ? rem_bits : 0;
    for (int current_bit = scalar_bits - 2 * window_bits; current_bit >= last_full_chunk_start; current_bit -= window_bits) {
        // Square window_bits times
        for (int j = 0; j < window_bits; j++) {
            result = ring.template batch_mulreduce<batch_size>(result, result);
        }
        // Get bits for this chunk
        int chunk_end = current_bit + window_bits - 1;
        std::array<int, batch_size> bits;
        for (int j = 0; j < batch_size; j++) {
            bits[j] = exponents[j].get_bits(current_bit, chunk_end);
        }
        // Select from window table and multiply
        std::array<typename Ring::RingElement, batch_size> op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(bits);
        } else {
            for (int j = 0; j < batch_size; j++) {
                op[j] = window_table[bits[j]];
            }
        }
        result = ring.template batch_mulreduce<batch_size>(result, op);
    }
    
    // Handle remainder chunk (if any) - get_bits handles out-of-bounds gracefully
    if (rem_bits > 0) {
        // Square rem_bits times (not window_bits times)
        for (int j = 0; j < rem_bits; j++) {
            result = ring.template batch_mulreduce<batch_size>(result, result);
        }
        // Get the remainder bits (get_bits handles bounds)
        std::array<int, batch_size> remainder_bits;
        for (int j = 0; j < batch_size; j++) {
            remainder_bits[j] = exponents[j].get_bits(0, rem_bits - 1);
        }
        std::array<typename Ring::RingElement, batch_size> op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(remainder_bits);
        } else {
            for (int j = 0; j < batch_size; j++) {
                op[j] = window_table[remainder_bits[j]];
            }
        }
        result = ring.template batch_mulreduce<batch_size>(result, op);
    }
    
    return result;
}

template<class Ring, int window_bits, int scalar_bits, int batch_size, bool constant_time = true, bool serialize_reduce = false>
std::array<typename Ring::RingElement, batch_size> fixed_window_exponentiate_batched_diffring(const std::array<typename Ring::RingElement, batch_size> &base, const std::array<ExponentScalar<scalar_bits>, batch_size> &exponents, const std::array<const Ring *, batch_size> &rings) {
    // Create window table
    FixedWindowTableBatchedMultiRing<Ring, window_bits, batch_size> window_table(base, rings);
    
    static_assert(window_bits <= scalar_bits, "window_bits must be <= scalar_bits");
    
    int rem_bits = scalar_bits % window_bits;
    
    // Start with the most significant chunk
    int first_chunk_start = scalar_bits - window_bits;
    std::array<int, batch_size> first_chunk_bits;
    for (int j = 0; j < batch_size; j++) {
        first_chunk_bits[j] = exponents[j].get_bits(first_chunk_start, scalar_bits - 1);
    }
    std::array<typename Ring::RingElement, batch_size> result = window_table.ctime_select(first_chunk_bits);
    
    // Process full chunks from MSB to LSB (skip remainder chunk)
    int last_full_chunk_start = (rem_bits > 0) ? rem_bits : 0;
    for (int current_bit = scalar_bits - 2 * window_bits; current_bit >= last_full_chunk_start; current_bit -= window_bits) {
        // Square window_bits times
        for (int j = 0; j < window_bits; j++) {
            result = Ring::template true_batch_mulreduce<batch_size, serialize_reduce>(rings, result, result);
        }
        // Get bits for this chunk
        int chunk_end = current_bit + window_bits - 1;
        std::array<int, batch_size> bits;
        for (int j = 0; j < batch_size; j++) {
            bits[j] = exponents[j].get_bits(current_bit, chunk_end);
        }
        // Select from window table and multiply
        std::array<typename Ring::RingElement, batch_size> op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(bits);
        } else {
            for (int j = 0; j < batch_size; j++) {
                op[j] = window_table[bits[j]];
            }
        }
        result = Ring::template true_batch_mulreduce<batch_size, serialize_reduce>(rings, result, op);
    }
    
    // Handle remainder chunk (if any) - get_bits handles out-of-bounds gracefully
    if (rem_bits > 0) {
        // Square rem_bits times (not window_bits times)
        for (int j = 0; j < rem_bits; j++) {
            result = Ring::template true_batch_mulreduce<batch_size, serialize_reduce>(rings, result, result);
        }
        // Get the remainder bits (get_bits handles bounds)
        std::array<int, batch_size> remainder_bits;
        for (int j = 0; j < batch_size; j++) {
            remainder_bits[j] = exponents[j].get_bits(0, rem_bits - 1);
        }
        std::array<typename Ring::RingElement, batch_size> op;
        if constexpr (constant_time == true) {
            op = window_table.ctime_select(remainder_bits);
        } else {
            for (int j = 0; j < batch_size; j++) {
                op[j] = window_table[remainder_bits[j]];
            }
        }
        result = Ring::template true_batch_mulreduce<batch_size, serialize_reduce>(rings, result, op);
    }
    
    return result;
}

template<class Ring>
typename Ring::RingElement exponentiate(const typename Ring::RingElement &base, const BigInt &exponent, const Ring &ring) {
    return exponentiate<Ring, Ring::modbits>(base, ExponentScalar<Ring::modbits>(exponent), ring);
}
