#!/bin/bash

cd "$(dirname "$0")"/..

./build/bssl_bench --benchmark_filter=BM_SpeedRSASign --benchmark_repetitions=5
