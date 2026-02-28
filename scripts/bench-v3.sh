#!/bin/bash

cd "$(dirname "$0")"/..

echo "Benchmarking with Clang 21..."
./build.clang21/bssl_bench --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/our_boringssl_clang21.txt --benchmark_out_format=console --benchmark_time_unit=us
echo "Benchmarking with Clang 15 (default)..."
./build/bssl_bench --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/our_boringssl_clang15.txt --benchmark_out_format=console --benchmark_time_unit=us
