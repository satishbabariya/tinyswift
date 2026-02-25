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

  // Two-pass approach: first collect all stores across ALL blocks to handle
  // the alloca+branch pattern (ternary, nil coalescing, optional chaining)
  // where stores happen in then/else blocks that may appear after the merge
  // block in linear order.

  // Map from StructElementAddr result id → the base alloca id it derives from.
  llvm::DenseMap<int32_t, int32_t> field_addr_to_alloca;

  // Pass 1: collect all stores and StructElementAddr derivations.
  for (const auto& bb : fn.blocks) {
    for (const auto& inst : bb->insts) {
      switch (inst->kind) {
        case TinySIL::SILInstKind::StructElementAddr: {
          if (inst->result.is_valid() && inst->operands[0].is_valid()) {
            auto base_id = inst->operands[0].id;
            if (alloc_states.count(base_id)) {
              field_addr_to_alloca[inst->result.id] = base_id;
            }
          }
          break;
        }

        case TinySIL::SILInstKind::Store: {
          auto dst = inst->operands[1];
          if (dst.is_valid()) {
            auto it = alloc_states.find(dst.id);
            if (it != alloc_states.end()) {
              it->second = InitState::Initialized;
            }
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

        default:
          break;
      }
    }
  }

  // Pass 2: check loads — only flag truly uninitialized allocas (those with
  // NO store anywhere in the function).
  for (const auto& bb : fn.blocks) {
    for (const auto& inst : bb->insts) {
      if (inst->kind == TinySIL::SILInstKind::Load) {
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
