// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_VERIFIER_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_VERIFIER_H_

#include "llvm/Support/raw_ostream.h"
#include "toolchain/tiny_sil/module.h"

namespace TinySwift::TinySIL {

// Verifies the structural integrity of a SIL module.
// Returns true if the module is valid, false if errors were found.
auto VerifySILModule(const SILModule& module,
                     llvm::raw_ostream* error_stream = nullptr) -> bool;

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_VERIFIER_H_
