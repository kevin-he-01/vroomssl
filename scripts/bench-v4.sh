#!/bin/bash

cd "$(dirname "$0")"/..

echo "Benchmarking with old version..."
./bssl_bench.old --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/jan21_old.txt --benchmark_out_format=console --benchmark_time_unit=us
echo "Benchmarking with new version..."
./build/bssl_bench --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_out=results/jan21_new.txt --benchmark_out_format=console --benchmark_time_unit=us
