// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/Support/Debug.h"
#include "toolchain/tiny_sil_optimizer/arc_elim.h"

#define DEBUG_TYPE "tinyswift-sil-optimizer"

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
  ARCEliminationStats total_stats;

  for (auto& fn : module.functions) {
    if (!fn->is_declaration && fn->hasBody()) {
      // Run passes in a fixpoint loop — each pass may enable others.
      constexpr int kMaxIterations = 4;
      for (int iter = 0; iter < kMaxIterations; ++iter) {
        bool changed = false;

        // Pass 1: Mem2Reg — promote alloc_stack to SSA values.
        changed |= RunMem2Reg(*fn);

        // Pass 2: ARC elimination — remove redundant retain/release pairs.
        RunARCElimination(*fn, &total_stats);

        // Pass 3: Dead code elimination — remove unused values.
        changed |= RunDeadCodeElimination(*fn);

        if (!changed) break;
      }
    }
  }

  LLVM_DEBUG(if (total_stats.retains_eliminated > 0 ||
                 total_stats.releases_eliminated > 0 ||
                 total_stats.moves_converted > 0) {
    llvm::dbgs() << "ARC elimination: " << total_stats.retains_eliminated
                 << " retains eliminated, " << total_stats.releases_eliminated
                 << " releases eliminated, " << total_stats.moves_converted
                 << " moves converted\n";
  });
}

}  // namespace TinySwift::TinySILOptimizer
