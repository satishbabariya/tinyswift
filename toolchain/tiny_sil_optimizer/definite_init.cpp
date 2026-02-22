// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "toolchain/tiny_sil/instruction.h"

namespace TinySwift::TinySILOptimizer {

namespace {

// Tracks the initialization state of alloc_stack values.
enum class InitState {
  Uninitialized,
  MaybeInitialized,
  Initialized,
};

// Checks a single function for use-before-initialization.
auto CheckDefiniteInit(const TinySIL::SILFunction& fn,
                       llvm::raw_ostream* error_stream) -> bool {
  bool success = true;

  // Collect alloc_stack values.
  llvm::DenseMap<int32_t, InitState> alloc_states;

  for (const auto& bb : fn.blocks) {
    for (const auto& inst : bb->insts) {
      if (inst->kind == TinySIL::SILInstKind::AllocStack) {
        if (inst->result.is_valid()) {
          alloc_states[inst->result.id] = InitState::Uninitialized;
        }
      }
    }
  }

  if (alloc_states.empty()) {
    return true;  // No alloc_stack — nothing to check.
  }

  // Map from StructElementAddr result id → the base alloca id it derives from.
  // Stores to these derived addresses count as partial initialization of the
  // base alloca (e.g., `self.x = v` in an init body initializes part of self).
  llvm::DenseMap<int32_t, int32_t> field_addr_to_alloca;

  // Walk instructions linearly (single-block approximation).
  // A full implementation would do a dominator-tree walk.
  for (const auto& bb : fn.blocks) {
    for (const auto& inst : bb->insts) {
      switch (inst->kind) {
        case TinySIL::SILInstKind::StructElementAddr: {
          // Track that this derived address comes from a base alloca.
          if (inst->result.is_valid() && inst->operands[0].is_valid()) {
            auto base_id = inst->operands[0].id;
            if (alloc_states.count(base_id)) {
              field_addr_to_alloca[inst->result.id] = base_id;
            }
          }
          break;
        }

        case TinySIL::SILInstKind::Store: {
          // store %src to %dst — marks dst as initialized.
          auto dst = inst->operands[1];
          if (dst.is_valid()) {
            // Direct store to alloca.
            auto it = alloc_states.find(dst.id);
            if (it != alloc_states.end()) {
              it->second = InitState::Initialized;
            }
            // Store to a StructElementAddr derived from an alloca —
            // mark the alloca as at least partially initialized.
            auto fa_it = field_addr_to_alloca.find(dst.id);
            if (fa_it != field_addr_to_alloca.end()) {
              auto alloca_it = alloc_states.find(fa_it->second);
              if (alloca_it != alloc_states.end() &&
                  alloca_it->second == InitState::Uninitialized) {
                alloca_it->second = InitState::MaybeInitialized;
              }
            }
          }
          break;
        }

        case TinySIL::SILInstKind::Load: {
          // load %addr — check that addr is initialized.
          auto addr = inst->operands[0];
          if (addr.is_valid()) {
            auto it = alloc_states.find(addr.id);
            if (it != alloc_states.end() &&
                it->second == InitState::Uninitialized) {
              success = false;
              if (error_stream) {
                *error_stream
                    << "error: use of uninitialized variable in function '@"
                    << fn.name << "': %" << addr.id
                    << " loaded before initialization\n";
              }
            }
          }
          break;
        }

        default:
          break;
      }
    }
  }

  return success;
}

}  // namespace

auto RunDefiniteInitialization(TinySIL::SILModule& module,
                               llvm::raw_ostream* error_stream) -> bool {
  bool success = true;
  for (const auto& fn : module.functions) {
    if (!fn->is_declaration && fn->hasBody()) {
      if (!CheckDefiniteInit(*fn, error_stream)) {
        success = false;
      }
    }
  }
  return success;
}

}  // namespace TinySwift::TinySILOptimizer
