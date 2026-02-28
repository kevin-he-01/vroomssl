#!/bin/bash

cd "$(dirname "$0")"/..

# sudo perf record -g --call-graph dwarf -o rsa_sign_2048.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSASign/2048 --benchmark_repetitions=10
# sudo perf record -g --call-graph dwarf -o rsa_sign_3072.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSASign/3072 --benchmark_repetitions=10
# sudo perf record -g --call-graph dwarf -o rsa_sign_4096.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSASign/4096 --benchmark_repetitions=10

# sudo perf record -g --call-graph dwarf -o rsa_verify_2048.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSAVerify/2048 --benchmark_repetitions=10
# sudo perf record -g --call-graph dwarf -o rsa_verify_3072.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSAVerify/3072 --benchmark_repetitions=10
# sudo perf record -g --call-graph dwarf -o rsa_verify_4096.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRSAVerify/4096 --benchmark_repetitions=10

sudo perf record -g --call-graph dwarf -o rns_sign_2048.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSASign/2048
sudo perf record -g --call-graph dwarf -o rns_sign_3072.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSASign/3072
sudo perf record -g --call-graph dwarf -o rns_sign_4096.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSASign/4096

sudo perf record -g --call-graph dwarf -o rns_verify_2048.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSAVerify/2048
sudo perf record -g --call-graph dwarf -o rns_verify_3072.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSAVerify/3072
sudo perf record -g --call-graph dwarf -o rns_verify_4096.perf -- ./build/bssl_bench --benchmark_filter=BM_SpeedRNSRSAVerify/4096
