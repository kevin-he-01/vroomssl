
#! /bin/bash

cd "$(dirname "$0")"/..

~/bssl_bench.clang15 --benchmark_filter='BM_RNSDifferentRingExponentiationBatched<1024, 3>|BM_RNSDifferentRingExponentiationBatched<1024, 4>|BM_RNSDifferentRingExponentiationBatched<1024, 5>|BM_RNSDifferentRingExponentiationBatched<1024, 6>|BM_RNSDifferentRingExponentiationBatched<1024, 7>|BM_RNSDifferentRingExponentiationBatched<1024, 8>|BM_RNSDifferentRingExponentiationBatched<1536, 3>|BM_RNSDifferentRingExponentiationBatched<1536, 4>|BM_RNSDifferentRingExponentiationBatched<1536, 5>|BM_RNSDifferentRingExponentiationBatched<1536, 6>|BM_RNSDifferentRingExponentiationBatched<1536, 7>|BM_RNSDifferentRingExponentiationBatched<1536, 8>|BM_RNSExponentiation<1024, 3>|BM_RNSExponentiation<1024, 4>|BM_RNSExponentiation<1024, 5>|BM_RNSExponentiation<1024, 6>|BM_RNSExponentiation<1024, 7>|BM_RNSExponentiation<1024, 8>|BM_RNSExponentiation<1536, 3>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<1536, 5>|BM_RNSExponentiation<1536, 6>|BM_RNSExponentiation<1536, 7>|BM_RNSExponentiation<1536, 8>|BM_RNSExponentiation<2048, 3>|BM_RNSExponentiation<2048, 4>|BM_RNSExponentiation<2048, 5>|BM_RNSExponentiation<2048, 6>|BM_RNSExponentiation<2048, 7>|BM_RNSExponentiation<2048, 8>' --benchmark_out=results/csv.clang15/rns_exponentiation.csv --benchmark_out_format=csv --benchmark_time_unit=us

~/bssl_bench.clang15 --benchmark_filter='BM_RNSInt2Mont<2048>|BM_RNSInt2Mont<3072>|BM_RNSInt2Mont<4096>|BM_RNSMont2Int<2048>|BM_RNSMont2Int<3072>|BM_RNSMont2Int<4096>|BM_RNSRingMultiplication<2048>|BM_RNSRingMultiplication<3072>|BM_RNSRingMultiplication<4096>' --benchmark_out=results/csv.clang15/rns_conversions.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=ns --benchmark_repetitions=5

# Ours and BoringSSL
~/bssl_bench.clang15 --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/csv.clang15/ours_and_boringssl.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=us --benchmark_repetitions=5

# Strip first 9 lines of each CSV file (headers)
sed -i '1,9d' results/csv.clang15/ours_and_boringssl.csv
sed -i '1,9d' results/csv.clang15/rns_conversions.csv
sed -i '1,9d' results/csv.clang15/rns_exponentiation.csv

~/bssl_bench.clang21 --benchmark_filter='BM_RNSDifferentRingExponentiationBatched<1024, 3>|BM_RNSDifferentRingExponentiationBatched<1024, 4>|BM_RNSDifferentRingExponentiationBatched<1024, 5>|BM_RNSDifferentRingExponentiationBatched<1024, 6>|BM_RNSDifferentRingExponentiationBatched<1024, 7>|BM_RNSDifferentRingExponentiationBatched<1024, 8>|BM_RNSDifferentRingExponentiationBatched<1536, 3>|BM_RNSDifferentRingExponentiationBatched<1536, 4>|BM_RNSDifferentRingExponentiationBatched<1536, 5>|BM_RNSDifferentRingExponentiationBatched<1536, 6>|BM_RNSDifferentRingExponentiationBatched<1536, 7>|BM_RNSDifferentRingExponentiationBatched<1536, 8>|BM_RNSExponentiation<1024, 3>|BM_RNSExponentiation<1024, 4>|BM_RNSExponentiation<1024, 5>|BM_RNSExponentiation<1024, 6>|BM_RNSExponentiation<1024, 7>|BM_RNSExponentiation<1024, 8>|BM_RNSExponentiation<1536, 3>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<1536, 5>|BM_RNSExponentiation<1536, 6>|BM_RNSExponentiation<1536, 7>|BM_RNSExponentiation<1536, 8>|BM_RNSExponentiation<2048, 3>|BM_RNSExponentiation<2048, 4>|BM_RNSExponentiation<2048, 5>|BM_RNSExponentiation<2048, 6>|BM_RNSExponentiation<2048, 7>|BM_RNSExponentiation<2048, 8>' --benchmark_out=results/csv.clang21/rns_exponentiation.csv --benchmark_out_format=csv --benchmark_time_unit=us

~/bssl_bench.clang21 --benchmark_filter='BM_RNSInt2Mont<2048>|BM_RNSInt2Mont<3072>|BM_RNSInt2Mont<4096>|BM_RNSMont2Int<2048>|BM_RNSMont2Int<3072>|BM_RNSMont2Int<4096>|BM_RNSRingMultiplication<2048>|BM_RNSRingMultiplication<3072>|BM_RNSRingMultiplication<4096>' --benchmark_out=results/csv.clang21/rns_conversions.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=ns --benchmark_repetitions=5

# Ours and BoringSSL
~/bssl_bench.clang21 --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/csv.clang21/ours_and_boringssl.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=us --benchmark_repetitions=5

# Strip first 9 lines of each CSV file (headers)
sed -i '1,9d' results/csv.clang21/ours_and_boringssl.csv
sed -i '1,9d' results/csv.clang21/rns_conversions.csv
sed -i '1,9d' results/csv.clang21/rns_exponentiation.csv

tput bel
sleep 1
tput bel
echo "Done. Please push the results to the repo."
