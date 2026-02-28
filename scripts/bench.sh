#! /bin/bash
# Contains what I am about to run

cd "$(dirname "$0")"

# ./bench-v3.sh
./bench-v4.sh
./profile-v2.sh

tput bel
sleep 1
tput bel
echo "Done. Please push the results to the repo."

