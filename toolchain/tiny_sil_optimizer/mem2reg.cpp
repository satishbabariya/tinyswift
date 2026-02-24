// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "toolchain/tiny_sil/instruction.h"

namespace TinySwift::TinySILOptimizer {

// Mem2Reg: Promotes alloc_stack locations that are only accessed within a
// single basic block to SSA values (removes the alloca/load/store pattern).
//
// This is a simplified version that handles the common case:
//   %addr = alloc_stack $T
//   store %val to %addr
//   %loaded = load %addr
//   dealloc_stack %addr
//
// After Mem2Reg:
//   The load is replaced with the stored value directly, and the
//   alloc_stack/store/load/dealloc_stack are removed.
auto RunMem2Reg(TinySIL::SILFunction& function) -> void {
  for (auto& bb : function.blocks) {
    // Find alloc_stack values that are only used within this block.
    llvm::DenseMap<int32_t, TinySIL::SILValue> promoted_values;
    llvm::DenseSet<int32_t> alloc_ids;

    // First pass: find alloc_stack instructions.
    for (const auto& inst : bb->insts) {
      if (inst->kind == TinySIL::SILInstKind::AllocStack &&
          inst->result.is_valid()) {
        alloc_ids.insert(inst->result.id);
      }
    }

    if (alloc_ids.empty()) {
      continue;
    }

    // Second pass: single forward scan to track stores and promote loads.
    // This must be a SINGLE pass so that each load is promoted to the value
    // stored MOST RECENTLY BEFORE that load — not the final stored value.
    // (Using separate "scan all stores" + "scan all loads" passes is wrong
    // for compound assignments like x += 5; x -= 3 where the same alloca is
    // stored multiple times.)
    llvm::DenseMap<int32_t, TinySIL::SILValue> current_value;
    llvm::DenseSet<int32_t> escaped;

    for (auto& inst : bb->insts) {
      // Check for address escapes (used as function argument).
      if (inst->kind == TinySIL::SILInstKind::Apply) {
        for (const auto& arg : inst->operand_list) {
          if (arg.is_valid() && alloc_ids.count(arg.id)) {
            escaped.insert(arg.id);
          }
        }
      }

      // Track stores: update the current value for this alloca.
      if (inst->kind == TinySIL::SILInstKind::Store) {
        auto dst = inst->operands[1];
        auto src = inst->operands[0];
        if (dst.is_valid() && src.is_valid() && alloc_ids.count(dst.id) &&
            !escaped.count(dst.id)) {
          current_value[dst.id] = src;
        }
      }

      // Promote loads: use the current (most recently stored) value.
      if (inst->kind == TinySIL::SILInstKind::Load) {
        auto addr = inst->operands[0];
        if (addr.is_valid()) {
          auto it = current_value.find(addr.id);
          if (it != current_value.end() && !escaped.count(addr.id)) {
            if (inst->result.is_valid()) {
              promoted_values[inst->result.id] = it->second;
            }
          }
        }
      }
    }

    // Fourth pass: propagate promoted values through the block.
    // Any instruction that uses a promoted value should use the original
    // stored value instead.
    if (!promoted_values.empty()) {
      for (auto& inst : bb->insts) {
        // Replace operands that reference promoted load results.
        for (int i = 0; i < inst->num_operands; ++i) {
          auto op = inst->operands[i];
          if (op.is_valid()) {
            auto it = promoted_values.find(op.id);
            if (it != promoted_values.end()) {
              inst->operands[i] = it->second;
            }
          }
        }
        for (auto& op : inst->operand_list) {
          if (op.is_valid()) {
            auto it = promoted_values.find(op.id);
            if (it != promoted_values.end()) {
              op = it->second;
            }
          }
        }
        for (auto& op : inst->branch_args) {
          if (op.is_valid()) {
            auto it = promoted_values.find(op.id);
            if (it != promoted_values.end()) {
              op = it->second;
            }
          }
        }
      }
    }
  }
}

}  // namespace TinySwift::TinySILOptimizer
