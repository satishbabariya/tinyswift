// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_PASS_MANAGER_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_PASS_MANAGER_H_

#include "llvm/Support/raw_ostream.h"
#include "toolchain/tiny_sil/module.h"
#include "toolchain/tiny_sil_optimizer/arc_elim.h"

namespace TinySwift::TinySILOptimizer {

// Runs all mandatory diagnostic passes on a SIL module.
// Returns true if no errors were found.
auto RunMandatoryPasses(TinySIL::SILModule& module,
                        llvm::raw_ostream* error_stream = nullptr) -> bool;

// Runs optional performance optimization passes on a SIL module.
auto RunPerformancePasses(TinySIL::SILModule& module) -> void;

// Individual mandatory passes.
auto RunDefiniteInitialization(TinySIL::SILModule& module,
                               llvm::raw_ostream* error_stream) -> bool;
auto RunReturnAnalysis(TinySIL::SILModule& module,
                       llvm::raw_ostream* error_stream) -> bool;

// Individual performance passes.
auto RunMem2Reg(TinySIL::SILFunction& function) -> void;
// RunARCElimination is declared in arc_elim.h (with optional stats parameter).
auto RunDeadCodeElimination(TinySIL::SILFunction& function) -> void;

}  // namespace TinySwift::TinySILOptimizer

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_OPTIMIZER_PASS_MANAGER_H_
