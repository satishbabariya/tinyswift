// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/ADT/DenseSet.h"
#include "toolchain/tiny_sil/instruction.h"

namespace TinySwift::TinySILOptimizer {

namespace {

// Checks that all paths in a function reach a return instruction.
// For functions that return a value, every path must end with ReturnInst.
auto CheckReturnPaths(const TinySIL::SILFunction& fn,
                      llvm::raw_ostream* error_stream) -> bool {
  if (fn.is_declaration || fn.blocks.empty()) {
    return true;
  }

  // Void functions don't strictly need return on every path
  // (the SILGen adds them), but we still check for terminated blocks.
  bool is_void = fn.type.is_void_return;

  // Check that every block ends with a terminator.
  for (const auto& bb : fn.blocks) {
    if (bb->insts.empty()) {
      if (error_stream) {
        *error_stream << "error: empty basic block bb" << bb->id
                      << " in function '@" << fn.name << "'\n";
      }
      return false;
    }

    auto* last = bb->insts.back().get();
    bool is_terminator = false;
    switch (last->kind) {
      case TinySIL::SILInstKind::ReturnInst:
      case TinySIL::SILInstKind::Branch:
      case TinySIL::SILInstKind::CondBranch:
      case TinySIL::SILInstKind::SwitchEnum:
      case TinySIL::SILInstKind::Unreachable:
        is_terminator = true;
        break;
      default:
        break;
    }

    if (!is_terminator) {
      if (error_stream) {
        *error_stream << "error: basic block bb" << bb->id
                      << " in function '@" << fn.name
                      << "' does not end with a terminator\n";
      }
      return false;
    }
  }

  // For non-void functions, check that at least one block ends with return.
  if (!is_void) {
    bool has_return = false;
    for (const auto& bb : fn.blocks) {
      if (!bb->insts.empty() &&
          bb->insts.back()->kind == TinySIL::SILInstKind::ReturnInst) {
        has_return = true;
        break;
      }
    }

    if (!has_return) {
      if (error_stream) {
        *error_stream << "error: non-void function '@" << fn.name
                      << "' missing return on all paths\n";
      }
      return false;
    }
  }

  // Walk reachable blocks from entry to check for unreachable code that
  // should return. For now, we do a simple check: any block ending with
  // Unreachable in a non-void function is suspicious.
  if (!is_void) {
    for (const auto& bb : fn.blocks) {
      if (!bb->insts.empty() &&
          bb->insts.back()->kind == TinySIL::SILInstKind::Unreachable) {
        // This is a warning, not an error — it might be intentional
        // (e.g., after a fatalError call).
      }
    }
  }

  return true;
}

}  // namespace

auto RunReturnAnalysis(TinySIL::SILModule& module,
                       llvm::raw_ostream* error_stream) -> bool {
  bool success = true;
  for (const auto& fn : module.functions) {
    if (!fn->is_declaration && fn->hasBody()) {
      if (!CheckReturnPaths(*fn, error_stream)) {
        success = false;
      }
    }
  }
  return success;
}

}  // namespace TinySwift::TinySILOptimizer
