// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_ARC_ELIM_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_ARC_ELIM_H_

#include <cstdint>

#include "toolchain/tiny_sil/module.h"

namespace TinySwift::TinySILOptimizer {

// Statistics collected during ARC elimination (M95/M96).
struct ARCEliminationStats {
  int32_t retains_eliminated = 0;
  int32_t releases_eliminated = 0;
  int32_t moves_converted = 0;
};

// Runs ARC elimination on a single SIL function.
// M95: Removes redundant retain/release pairs for borrow-only uses.
// M96: Converts retain/release to moves for last-use patterns.
//
// If `stats` is non-null, populates it with counts of eliminated operations.
auto RunARCElimination(TinySIL::SILFunction& function,
                       ARCEliminationStats* stats = nullptr) -> bool;

}  // namespace TinySwift::TinySILOptimizer

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_ARC_ELIM_H_
