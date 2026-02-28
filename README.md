# Modified BoringSSL for VROOM

## WARNING: This is not a production-ready library.

This is software for a research prototype. Please
do *NOT* use this code in production.


We only performed minimal testing that are necessary to ensure the validity of our benchmark claims (to the best of our knowledge), using only _random_ inputs to _select_ functions.
Therefore, the software may contain (but is not limited to) memory leaks, memory errors (e.g., null pointer dereference), and other (security) bugs/vulnerabilities, especially when used as a library in untested use cases.

## Background

This is an artifact repository for "VROOM: Accelerating (Almost All) Number-Theoretic Cryptography Using Vectorization and the Residue Number System", a new modular multiplication algorithm based on a work in submission, and will appear soon in the [Cryptology ePrint Archive](https://eprint.iacr.org/).

In this artifact, we forked [commit `3fb010b`](https://github.com/google/boringssl/commit/3fb010b032008c2da1e9c1d16bccf774d46c5211) from upstream BoringSSL, and modified it to use VROOM for modular multiplication.

## Prerequisites

For convenience, this repository also benchmarks [FLINT](https://flintlib.org/) as a baseline, which means that you need to ensure FLINT and [GMP](https://gmplib.org/) are in your system as dependencies, in addition to all dependencies required by unmodified BoringSSL.
Note our VROOM code _does not_ rely on GMP or FLINT, instead using subroutines in BoringSSL (`BN_*`), and carefully ensured that we call only constant-time routines in BoringSSL in code that does multiplication and conversions between radix and RNS form, to the best of our knowledge.

## Build instructions

We tested the build using `clang` 21.1.8 compiled from source, running Amazon Linux 2023 on an AWS c7i.metal-24xl instance.

To build the library for benchmarking, set environment variables `CC` and `CXX` to point to the `clang` and `clang++` binaries (version 21.1.8), respectively, and pass the **release** build option to CMake (very important to ensure you can reproduce the accurate benchmark results):
```
export CC=clang
export CXX=clang++
cmake -GNinja -B build -DCMAKE_BUILD_TYPE=Release
```
Then, to reproduce our evaluation results, run the benchmark binary `./build/bssl_bench` from the repository root.
This will provide all the performance result of the original BoringSSL codepath in addition to that of VROOM.

The core VROOM code can be found in the subfolder [rns/](/rns/), and we made modifications to BoringSSL library sources that handle RSA signing and verification.

## BoringSSL (original project README)

BoringSSL is a fork of OpenSSL that is designed to meet Google's needs.

Although BoringSSL is an open source project, it is not intended for general
use, as OpenSSL is. We don't recommend that third parties depend upon it. Doing
so is likely to be frustrating because there are no guarantees of API or ABI
stability.

Programs ship their own copies of BoringSSL when they use it and we update
everything as needed when deciding to make API changes. This allows us to
mostly avoid compromises in the name of compatibility. It works for us, but it
may not work for you.

BoringSSL arose because Google used OpenSSL for many years in various ways and,
over time, built up a large number of patches that were maintained while
tracking upstream OpenSSL. As Google's product portfolio became more complex,
more copies of OpenSSL sprung up and the effort involved in maintaining all
these patches in multiple places was growing steadily.

Currently BoringSSL is the SSL library in Chrome/Chromium, Android (but it's
not part of the NDK) and a number of other apps/programs.

Project links:

  * [API documentation](https://commondatastorage.googleapis.com/chromium-boringssl-docs/headers.html)
  * [Issue tracker](https://crbug.com/boringssl)
    * [Filing new (public) issues](https://crbug.com/boringssl/new)
  * [CI](https://ci.chromium.org/p/boringssl/g/main/console)
  * [Code review](https://boringssl-review.googlesource.com)

To file a security issue, use the [Chromium process](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/) and mention in the report this is for BoringSSL. You can ignore the parts of the process that are specific to Chromium/Chrome.

There are other files in this directory which might be helpful:

  * [PORTING.md](./PORTING.md): how to port OpenSSL-using code to BoringSSL.
  * [BUILDING.md](./BUILDING.md): how to build BoringSSL
  * [INCORPORATING.md](./INCORPORATING.md): how to incorporate BoringSSL into a project.
  * [API-CONVENTIONS.md](./API-CONVENTIONS.md): general API conventions for BoringSSL consumers and developers.
  * [STYLE.md](./STYLE.md): rules and guidelines for coding style.
  * include/openssl: public headers with API documentation in comments. Also [available online](https://commondatastorage.googleapis.com/chromium-boringssl-docs/headers.html).
  * [FUZZING.md](./FUZZING.md): information about fuzzing BoringSSL.
  * [CONTRIBUTING.md](./CONTRIBUTING.md): how to contribute to BoringSSL.
  * [BREAKING-CHANGES.md](./BREAKING-CHANGES.md): notes on potentially-breaking changes.
  * [SANDBOXING.md](./SANDBOXING.md): notes on using BoringSSL in a sandboxed environment.
