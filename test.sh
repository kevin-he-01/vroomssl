#! /bin/bash
taskset -c 0 ./build/bssl_bench --benchmark_list_tests | grep 'BM_RNSRingMultiplication'
taskset -c 0 ./build/bssl_bench --benchmark_filter='BM_SpeedRSASign|BM_SpeedRNSRSASign|BM_SpeedRSAVerify|BM_SpeedRNSRSAVerify' --benchmark_time_unit=us

