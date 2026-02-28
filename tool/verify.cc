// Copyright 2017 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <map>
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "internal.h"


static const struct argument kArguments[] = {
    {"-key", kRequiredArgument, "The public key, in PEM format, to verify with"},
    {"-digest", kOptionalArgument, "The digest algorithm to use"},
    {"-rns", kBooleanArgument, "Use RNS Montgomery for exponentiation"},
    {"-in", kOptionalArgument, "Input file containing data to verify (default: stdin)"},
    {"-signature", kOptionalArgument, "File containing signature to verify (default: stdin)"},
    {"", kOptionalArgument, ""},
};

bool Verify(const std::vector<std::string> &args) {
  std::map<std::string, std::string> args_map;
  if (!ParseKeyValueArguments(&args_map, args, kArguments)) {
    PrintUsage(kArguments);
    return false;
  }

  int use_rns = 0;
  if (args_map.count("-rns")) {
    use_rns = 1;
  }
  RSA_set_use_rns(use_rns);

  // Load the public key.
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), args_map["-key"].c_str())) {
    return false;
  }
  bssl::UniquePtr<EVP_PKEY> key(
      PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
  if (!key) {
    return false;
  }

  const EVP_MD *md = nullptr;
  if (args_map.count("-digest")) {
    md = EVP_get_digestbyname(args_map["-digest"].c_str());
    if (md == nullptr) {
      fprintf(stderr, "Unknown digest algorithm: %s\n",
              args_map["-digest"].c_str());
      return false;
    }
  }

  // Read the signature.
  std::vector<uint8_t> sig;
  if (args_map.count("-signature")) {
    ScopedFILE sig_file(fopen(args_map["-signature"].c_str(), "rb"));
    if (!sig_file) {
      fprintf(stderr, "Error opening signature file: %s\n",
              args_map["-signature"].c_str());
      return false;
    }
    if (!ReadAll(&sig, sig_file.get())) {
      fprintf(stderr, "Error reading signature.\n");
      return false;
    }
  } else {
    if (!ReadAll(&sig, stdin)) {
      fprintf(stderr, "Error reading signature from stdin.\n");
      return false;
    }
  }

  // Read the data to verify.
  std::vector<uint8_t> data;
  if (args_map.count("-in")) {
    ScopedFILE data_file(fopen(args_map["-in"].c_str(), "rb"));
    if (!data_file) {
      fprintf(stderr, "Error opening input file: %s\n",
              args_map["-in"].c_str());
      return false;
    }
    if (!ReadAll(&data, data_file.get())) {
      fprintf(stderr, "Error reading input data.\n");
      return false;
    }
  } else {
    // If no -in is specified, read data from stdin.
    // However, if signature is also from stdin, we can't read both.
    // So require -signature to be specified if we're reading data from stdin.
    if (!args_map.count("-signature")) {
      fprintf(stderr, "Error: cannot read both signature and data from stdin. "
                      "Use -signature or -in to specify a file.\n");
      return false;
    }
    if (!ReadAll(&data, stdin)) {
      fprintf(stderr, "Error reading input data from stdin.\n");
      return false;
    }
  }

  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), nullptr, md, nullptr, key.get())) {
    return false;
  }

  int ret = EVP_DigestVerify(ctx.get(), sig.data(), sig.size(), data.data(),
                             data.size());

// Uncomment to see Montgomery context being reused (saving computational cost)
//   if (!EVP_DigestVerifyInit(ctx.get(), nullptr, md, nullptr, key.get())) {
//     return false;
//   }

//   ret &= EVP_DigestVerify(ctx.get(), sig.data(), sig.size(), data.data(),
//                              data.size());

  if (ret == 1) {
    fprintf(stdout, "Signature verified successfully.\n");
    return true;
  } else if (ret == 0) {
    fprintf(stderr, "Signature verification failed.\n");
    return false;
  } else {
    // Error during verification
    return false;
  }
}
