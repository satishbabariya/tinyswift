// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/ADT/DenseSet.h"
#include "toolchain/tiny_sil/instruction.h"

namespace TinySwift::TinySILOptimizer {

namespace {

// Returns true if an instruction has side effects (should not be removed).
auto HasSideEffects(TinySIL::SILInstKind kind) -> bool {
  switch (kind) {
    // Memory writes.
    case TinySIL::SILInstKind::Store:
    case TinySIL::SILInstKind::AllocStack:
    case TinySIL::SILInstKind::DeallocStack:
    case TinySIL::SILInstKind::AllocBox:
    // Function calls.
    case TinySIL::SILInstKind::Apply:
    case TinySIL::SILInstKind::PartialApply:
    // Terminators.
    case TinySIL::SILInstKind::Branch:
    case TinySIL::SILInstKind::CondBranch:
    case TinySIL::SILInstKind::SwitchEnum:
    case TinySIL::SILInstKind::ReturnInst:
    case TinySIL::SILInstKind::Unreachable:
    // Ownership.
    case TinySIL::SILInstKind::DestroyValue:
    case TinySIL::SILInstKind::EndBorrow:
    // Debug.
    case TinySIL::SILInstKind::DebugValue:
      return true;
    default:
      return false;
  }
}

}  // namespace

// Dead Code Elimination: removes instructions whose results are never used.
auto RunDeadCodeElimination(TinySIL::SILFunction& function) -> void {
  // Collect all used value IDs.
  llvm::DenseSet<int32_t> used_ids;

  for (const auto& bb : function.blocks) {
    for (const auto& inst : bb->insts) {
      // Mark operands as used.
      for (int i = 0; i < inst->num_operands; ++i) {
        if (inst->operands[i].is_valid()) {
          used_ids.insert(inst->operands[i].id);
        }
      }
      for (const auto& op : inst->operand_list) {
        if (op.is_valid()) {
          used_ids.insert(op.id);
        }
      }
      for (const auto& op : inst->branch_args) {
        if (op.is_valid()) {
          used_ids.insert(op.id);
        }
      }
    }
  }

  // Remove instructions that produce unused values and have no side effects.
  for (auto& bb : function.blocks) {
    llvm::SmallVector<std::unique_ptr<TinySIL::SILInstruction>> kept;
    for (auto& inst : bb->insts) {
      bool should_keep = true;

      if (inst->result.is_valid() && !HasSideEffects(inst->kind)) {
        // Check if the result is used anywhere.
        if (used_ids.count(inst->result.id) == 0) {
          should_keep = false;
        }
      }

      if (should_keep) {
        kept.push_back(std::move(inst));
      }
    }
    bb->insts = std::move(kept);
  }
}

}  // namespace TinySwift::TinySILOptimizer
