# Additional baselines

For reproducibility, we provided the code we used to benchmark additional baselines, including OpenSSL and [`arkworks`](https://arkworks.rs/). [FLINT](https://flintlib.org/) benchmarks are not included in this directory, instead found in [bench/flint.cc](/bench/flint.cc)

**OpenSSL.&emsp;** Since we modified OpenSSL to expose a few internal symbols, we include a patch of all such changes in [0001-expose-internal-symbols-Google-benchmarks.patch](./0001-expose-internal-symbols-Google-benchmarks.patch).
The patch includes all the benchmark code as a [CMake](https://cmake.org/) project in `bench/`.
The _high level_ list of steps is as follows (commands not tested, your results may vary):
1. Apply the patch on top of [OpenSSL commit `7a53925`](https://github.com/openssl/openssl/commit/7a53925198d2741a619a635048dce0095935168f)
2. Ensure clang 21.1.8 is installed in `/opt/llvm21/` (Edit `./build_openssl.sh` if it's not the case)
3. Run `./build_openssl.sh` to build the modified OpenSSL library.
4. Build the CMake project in `bench/`.

**arkworks.&emsp;** The `arkworks` benchmarks can be found in the [arkworks](./arkworks/) subfolder.
