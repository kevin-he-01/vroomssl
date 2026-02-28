#!/bin/bash

cd "$(dirname "$0")"

# ../build/bssl genrsa -bits 2048 > priv2048.pem
# ../build/bssl genrsa -bits 3072 > priv3072.pem
# ../build/bssl genrsa -bits 4096 > priv4096.pem

echo "##########################" >&2
echo "## Non-RNS Verification ##" >&2
echo "##########################" >&2
echo "Testing 2048-bit RSA" >&2
../build/bssl verify -key pub2048.pem -digest sha256 -in msg.txt -signature sign2048.bin
echo "Testing 3072-bit RSA" >&2
../build/bssl verify -key pub3072.pem -digest sha256 -in msg.txt -signature sign3072.bin
echo "Testing 4096-bit RSA" >&2
../build/bssl verify -key pub4096.pem -digest sha256 -in msg.txt -signature sign4096.bin

echo "##########################" >&2
echo "## RNS Verification     ##" >&2
echo "##########################" >&2
echo "Testing 2048-bit RSA" >&2
../build/bssl verify -rns -key pub2048.pem -digest sha256 -in msg.txt -signature sign2048.bin
echo "Testing 3072-bit RSA" >&2
../build/bssl verify -rns -key pub3072.pem -digest sha256 -in msg.txt -signature sign3072.bin
echo "Testing 4096-bit RSA" >&2
../build/bssl verify -rns -key pub4096.pem -digest sha256 -in msg.txt -signature sign4096.bin
