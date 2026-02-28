#!/bin/bash

cd "$(dirname "$0")"

# ../build/bssl genrsa -bits 2048 > priv2048.pem
# ../build/bssl genrsa -bits 3072 > priv3072.pem
# ../build/bssl genrsa -bits 4096 > priv4096.pem

echo "Testing 2048-bit RSA" >&2
../build/bssl sign -key priv2048.pem -digest sha256 < msg.txt > sign2048.bin
../build/bssl sign -key priv2048.pem -digest sha256 -rns < msg.txt > sign2048.bin-rns.bin
echo "Testing 3072-bit RSA" >&2
../build/bssl sign -key priv3072.pem -digest sha256 < msg.txt > sign3072.bin
../build/bssl sign -key priv3072.pem -digest sha256 -rns < msg.txt > sign3072.bin-rns.bin
echo "Testing 4096-bit RSA" >&2
../build/bssl sign -key priv4096.pem -digest sha256 < msg.txt > sign4096.bin
../build/bssl sign -key priv4096.pem -digest sha256 -rns < msg.txt > sign4096.bin-rns.bin

diff -s sign2048.bin sign2048.bin-rns.bin
diff -s sign3072.bin sign3072.bin-rns.bin
diff -s sign4096.bin sign4096.bin-rns.bin
