
#! /bin/bash

cd "$(dirname "$0")"/..

# ./build/bssl_bench --benchmark_filter='BM_SpeedRNSRSASign|BM_RNSDifferentRingExponentiationBatched<1024, 4>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<2048, 4>' --benchmark_out=results/json/ours.json --benchmark_repetitions=3

# Ours and BoringSSL
./build/bssl_bench --benchmark_filter='BM_ModExpConsttimeOG|BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_RNSDifferentRingExponentiationBatched<1024, 4>|BM_RNSExponentiation<1536, 4>|BM_RNSExponentiation<2048, 4>' --benchmark_out=results/json/ours_and_boringssl.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=us --benchmark_repetitions=5

# BoringSSL:
# - BM_ModExpConsttimeOG (x2)
# - BM_SpeedRSASign

# Ours: everything else

# OpenSSL:
../openssl/build/bench --benchmark_filter='BM_ModExpConsttime_x2|BM_RSASign_EVP_PKEY_sign' --benchmark_out=results/json/openssl.csv --benchmark_display_aggregates_only=true --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_time_unit=us --benchmark_repetitions=5

tput bel
sleep 1
tput bel
echo "Done. Please push the results to the repo."
