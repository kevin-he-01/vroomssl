
#! /bin/bash

cd "$(dirname "$0")"/..

~/bssl_bench.clang15 --benchmark_filter='ctime_select|BM_RNSRingMultiplication<1024>|BM_RNSRingMultiplication<1536>|BM_RNSRingMultiplication<2048>|BM_RNSExponentiation<1024, 1>|BM_RNSExponentiation<1024, 2>|BM_RNSExponentiation<1024, 3>|BM_RNSExponentiation<1024, 4>|BM_RNSExponentiation<1024, 5>|BM_RNSExponentiation<1024, 6>|BM_RNSExponentiation<1024, 7>|BM_RNSExponentiation<1024, 8>|BM_RNSExponentiation<1536, 1>|BM_RNSExponentiation<1536, 2>|BM_RNSExponentiation<1536, 3>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<1536, 5>|BM_RNSExponentiation<1536, 6>|BM_RNSExponentiation<1536, 7>|BM_RNSExponentiation<1536, 8>|BM_RNSExponentiation<2048, 1>|BM_RNSExponentiation<2048, 2>|BM_RNSExponentiation<2048, 3>|BM_RNSExponentiation<2048, 4>|BM_RNSExponentiation<2048, 5>|BM_RNSExponentiation<2048, 6>|BM_RNSExponentiation<2048, 7>|BM_RNSExponentiation<2048, 8>' --benchmark_out=results/csv.clang15/slowdown_mystery.csv --benchmark_out_format=csv --benchmark_time_unit=ns

# Strip first 9 lines of each CSV file (headers)
sed -i '1,9d' results/csv.clang15/slowdown_mystery.csv

~/bssl_bench.clang21 --benchmark_filter='ctime_select|BM_RNSRingMultiplication<1024>|BM_RNSRingMultiplication<1536>|BM_RNSRingMultiplication<2048>|BM_RNSExponentiation<1024, 1>|BM_RNSExponentiation<1024, 2>|BM_RNSExponentiation<1024, 3>|BM_RNSExponentiation<1024, 4>|BM_RNSExponentiation<1024, 5>|BM_RNSExponentiation<1024, 6>|BM_RNSExponentiation<1024, 7>|BM_RNSExponentiation<1024, 8>|BM_RNSExponentiation<1536, 1>|BM_RNSExponentiation<1536, 2>|BM_RNSExponentiation<1536, 3>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<1536, 5>|BM_RNSExponentiation<1536, 6>|BM_RNSExponentiation<1536, 7>|BM_RNSExponentiation<1536, 8>|BM_RNSExponentiation<2048, 1>|BM_RNSExponentiation<2048, 2>|BM_RNSExponentiation<2048, 3>|BM_RNSExponentiation<2048, 4>|BM_RNSExponentiation<2048, 5>|BM_RNSExponentiation<2048, 6>|BM_RNSExponentiation<2048, 7>|BM_RNSExponentiation<2048, 8>' --benchmark_out=results/csv.clang21/slowdown_mystery.csv --benchmark_out_format=csv --benchmark_time_unit=ns

# Strip first 9 lines of each CSV file (headers)
sed -i '1,9d' results/csv.clang21/slowdown_mystery.csv
