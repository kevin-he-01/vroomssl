#!/bin/bash

cd "$(dirname "$0")"/..

sudo perf record -g --call-graph dwarf -o rns_modexp_1024.perf -- ./build/bssl_bench --benchmark_filter=BM_ModExpConsttimeRNS/1024 --benchmark_repetitions=10
sudo perf record -g --call-graph dwarf -o rns_modexp_1536.perf -- ./build/bssl_bench --benchmark_filter=BM_ModExpConsttimeRNS/1536 --benchmark_repetitions=10
sudo perf record -g --call-graph dwarf -o rns_modexp_2048.perf -- ./build/bssl_bench --benchmark_filter=BM_ModExpConsttimeRNS/2048 --benchmark_repetitions=10

touch log.txt
OUT="$(realpath log.txt)"

# Serve as a reference
./build/bssl_bench '--benchmark_filter=BM_RNSRingMultiplication' >> "$OUT"
./build/bssl_bench '--benchmark_filter=BM_ModExpConsttimeRNS' >> "$OUT"

cd /home/ec2-user/RNSMont/scripts
python3 cpu.py 2 10000 1024 >> "$OUT"
python3 cpu.py 2 10000 1536 >> "$OUT"
python3 cpu.py 2 10000 2048 >> "$OUT"
