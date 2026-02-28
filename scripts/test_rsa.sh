#! /bin/bash

cd "$(dirname "$0")"/..

./build/crypto_test --gtest_filter='EVPTest.RSATestVectors*'
# EVPTest.RSATestVectors | EVPTest.RSATestVectorsRNS
