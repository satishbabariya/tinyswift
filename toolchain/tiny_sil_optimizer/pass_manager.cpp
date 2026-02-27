// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

namespace TinySwift::TinySILOptimizer {

auto RunMandatoryPasses(TinySIL::SILModule& module,
                        llvm::raw_ostream* error_stream) -> bool {
  bool success = true;

  // Pass 1: Definite initialization — ensure variables are initialized
  // before use.
  if (!RunDefiniteInitialization(module, error_stream)) {
    success = false;
  }

  // Pass 2: Return analysis — ensure all paths return a value.
  if (!RunReturnAnalysis(module, error_stream)) {
    success = false;
  }

  return success;
}

auto RunPerformancePasses(TinySIL::SILModule& module) -> void {
  for (auto& fn : module.functions) {
    if (!fn->is_declaration && fn->hasBody()) {
      // Pass 1: Mem2Reg — promote alloc_stack to SSA values.
      RunMem2Reg(*fn);

      // Pass 2: ARC elimination — remove redundant retain/release pairs (M95/M96).
      RunARCElimination(*fn);

      // Pass 3: Dead code elimination — remove unused values.
      RunDeadCodeElimination(*fn);
    }
  }
}

}  // namespace TinySwift::TinySILOptimizer
