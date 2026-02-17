// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_SIL_GEN_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_SIL_GEN_H_

#include <memory>

#include "toolchain/sem_ir/file.h"
#include "toolchain/tiny_sil/module.h"

namespace TinySwift::TinySILGen {

// Generates a TinySIL module from a SemIR file.
auto GenerateSIL(const SemIR::File& sem_ir)
    -> std::unique_ptr<TinySIL::SILModule>;

}  // namespace TinySwift::TinySILGen

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_SIL_GEN_H_
