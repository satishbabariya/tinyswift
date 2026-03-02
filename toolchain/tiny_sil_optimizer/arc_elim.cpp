// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// M95: Compile-time RC elimination — removes redundant retain/release pairs
//      for values that are only borrowed (read-only uses).
// M96: Move semantics — eliminates retain/release when a value is copied
//      to a new binding and the original is not used after the copy.

#include "toolchain/tiny_sil_optimizer/arc_elim.h"

#include "toolchain/tiny_sil_optimizer/pass_manager.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/tiny_sil/instruction.h"

namespace TinySwift::TinySILOptimizer {

namespace {

// Tracks a retain or release instruction and its location.
struct ARCInst {
  TinySIL::SILInstruction* inst;
  int32_t block_index;
  int32_t inst_index;
};

// Returns true if the given builtin name is a borrow-only use (never stores
// or escapes the pointer).
auto IsBorrowBuiltin(llvm::StringRef name) -> bool {
  return name.starts_with("print_") ||
         name == "string_eq" ||
         name == "string_len" ||
         name == "string_compare" ||
         name == "string_hash" ||
         name.starts_with("string_has_") ||
         name == "string_contains" ||
         name == "int_to_string" ||
         name == "is_unique";
}

// Classification of a use of a class-typed value.
enum class UseKind {
  Borrow,   // Read-only use — retain not needed.
  Escape,   // Value may be stored or transferred — retain needed.
  ARCPair,  // Part of retain/release pair — not counted as a real use.
};

// Classifies how a given value_id is used by an instruction.
auto ClassifyUse(const TinySIL::SILInstruction& user, int32_t value_id)
    -> UseKind {
  switch (user.kind) {
    case TinySIL::SILInstKind::StructExtract:
    case TinySIL::SILInstKind::TupleExtract:
    case TinySIL::SILInstKind::StructElementAddr:
    case TinySIL::SILInstKind::TupleElementAddr:
    case TinySIL::SILInstKind::DebugValue:
    case TinySIL::SILInstKind::BeginBorrow:
    case TinySIL::SILInstKind::EndBorrow:
    case TinySIL::SILInstKind::CopyValue:
      return UseKind::Borrow;

    case TinySIL::SILInstKind::BuiltinInst: {
      llvm::StringRef name(user.builtin_name);
      // Retain/release are ARC pair instructions, not real uses.
      if (name == "retain" || name == "release" || name == "release_cycle") {
        return UseKind::ARCPair;
      }
      if (IsBorrowBuiltin(name)) {
        return UseKind::Borrow;
      }
      // alloc_class result is the value itself, not a use of another value.
      if (name == "alloc_class") {
        return UseKind::Borrow;
      }
      // Conservative: any other builtin may escape.
      return UseKind::Escape;
    }

    case TinySIL::SILInstKind::Store:
      // If the value is being stored (operand[0] == value), it escapes.
      // If the value is the destination (operand[1] == value), it's a borrow.
      if (user.operands[0].is_valid() && user.operands[0].id == value_id) {
        return UseKind::Escape;
      }
      return UseKind::Borrow;

    case TinySIL::SILInstKind::Apply:
    case TinySIL::SILInstKind::PartialApply:
      // Conservative: function calls may store the value.
      return UseKind::Escape;

    case TinySIL::SILInstKind::ReturnInst:
      return UseKind::Escape;

    case TinySIL::SILInstKind::Branch:
    case TinySIL::SILInstKind::CondBranch:
    case TinySIL::SILInstKind::SwitchEnum:
      // Check if value appears in branch_args — that's an escape.
      for (const auto& arg : user.branch_args) {
        if (arg.is_valid() && arg.id == value_id) {
          return UseKind::Escape;
        }
      }
      return UseKind::Borrow;

    default:
      return UseKind::Borrow;
  }
}

// Returns true if the instruction at (block_index, inst_index) is a
// BuiltinInst("alloc_class") that produces the given value_id.
auto IsAllocClassProducer(const TinySIL::SILFunction& function,
                          int32_t value_id) -> bool {
  for (const auto& bb : function.blocks) {
    for (const auto& inst : bb->insts) {
      if (inst->result.is_valid() && inst->result.id == value_id &&
          inst->kind == TinySIL::SILInstKind::BuiltinInst &&
          inst->builtin_name == "alloc_class") {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

auto RunARCElimination(TinySIL::SILFunction& function,
                       ARCEliminationStats* stats) -> bool {
  // ═══════════════════════════════════════════════════════════════════════════
  // Step 1: Collect all retain and release instructions, indexed by the
  //         value_id of their operand[0] (the class-typed pointer).
  // ═══════════════════════════════════════════════════════════════════════════
  llvm::DenseMap<int32_t, llvm::SmallVector<ARCInst>> retains;  // value_id → [retain]
  llvm::DenseMap<int32_t, llvm::SmallVector<ARCInst>> releases; // value_id → [release]

  for (int32_t bi = 0; bi < static_cast<int32_t>(function.blocks.size()); ++bi) {
    auto& bb = *function.blocks[bi];
    for (int32_t ii = 0; ii < static_cast<int32_t>(bb.insts.size()); ++ii) {
      auto& inst = *bb.insts[ii];
      if (inst.kind != TinySIL::SILInstKind::BuiltinInst) continue;

      if (inst.builtin_name == "retain" && inst.operands[0].is_valid()) {
        retains[inst.operands[0].id].push_back(
            {&inst, bi, ii});
      } else if ((inst.builtin_name == "release" ||
                  inst.builtin_name == "release_cycle") &&
                 inst.operands[0].is_valid()) {
        releases[inst.operands[0].id].push_back(
            {&inst, bi, ii});
      }
    }
  }

  // If no retains, nothing to optimize.
  if (retains.empty()) return false;

  // ═══════════════════════════════════════════════════════════════════════════
  // Step 2: For each value that has both retains AND releases, check if
  //         all other uses are borrows. If so, mark the pairs for elimination.
  // ═══════════════════════════════════════════════════════════════════════════
  llvm::DenseSet<TinySIL::SILInstruction*> to_remove;

  for (auto& [value_id, retain_list] : retains) {
    auto release_it = releases.find(value_id);
    if (release_it == releases.end()) continue;
    auto& release_list = release_it->second;

    // Safety: never eliminate the ownership release for an alloc_class value.
    // The ownership release is the one paired with alloc_class (not with a
    // retain). We only eliminate copy-retains and their paired copy-releases.
    bool is_alloc_class = IsAllocClassProducer(function, value_id);

    // If this value was produced by alloc_class, the releases are ownership
    // releases (trigger deinit + free). We must NOT eliminate them.
    // Copy-retains for alloc_class values have the SAME operand[0] value_id,
    // but the releases are scope-exit releases. We need to check that we
    // only remove retain+release pairs that are COPIES, not the original.
    //
    // For alloc_class values: retains are copy-retains, but the releases
    // include BOTH copy-releases AND the original ownership release.
    // We can only eliminate retain+release pairs if ALL non-ARC uses are
    // borrows, AND we leave at least one release (the ownership one).
    if (is_alloc_class) {
      // For the original allocation, we need at least one release to remain
      // (the ownership release that fires deinit). Only eliminate if we have
      // more releases than retains (the extra releases are the ownership ones).
      if (release_list.size() <= retain_list.size()) {
        // All releases are paired with retains — can't remove any safely.
        // Actually this means every release has a matching retain, so the
        // ownership release must be among them. Skip this value.
        continue;
      }
    }

    // Check all uses of value_id across all blocks.
    bool all_borrows = true;
    for (const auto& bb : function.blocks) {
      for (const auto& inst : bb->insts) {
        // Check direct operands.
        for (int i = 0; i < inst->num_operands; ++i) {
          if (inst->operands[i].is_valid() && inst->operands[i].id == value_id) {
            auto kind = ClassifyUse(*inst, value_id);
            if (kind == UseKind::Escape) {
              all_borrows = false;
              break;
            }
          }
        }
        if (!all_borrows) break;

        // Check operand_list (Apply args, Struct fields, etc.).
        for (const auto& op : inst->operand_list) {
          if (op.is_valid() && op.id == value_id) {
            auto kind = ClassifyUse(*inst, value_id);
            if (kind == UseKind::Escape) {
              all_borrows = false;
              break;
            }
          }
        }
        if (!all_borrows) break;

        // Check branch_args.
        for (const auto& arg : inst->branch_args) {
          if (arg.is_valid() && arg.id == value_id) {
            auto kind = ClassifyUse(*inst, value_id);
            if (kind == UseKind::Escape) {
              all_borrows = false;
              break;
            }
          }
        }
        if (!all_borrows) break;
      }
      if (!all_borrows) break;
    }

    if (all_borrows) {
      // Mark all copy-retains for removal.
      for (auto& r : retain_list) {
        to_remove.insert(r.inst);
        if (stats) { ++stats->retains_eliminated; }
      }

      if (is_alloc_class) {
        // Remove only as many releases as retains (keep the ownership release).
        size_t to_elim = retain_list.size();
        for (size_t i = 0; i < to_elim && i < release_list.size(); ++i) {
          to_remove.insert(release_list[i].inst);
          if (stats) { ++stats->releases_eliminated; }
        }
      } else {
        // Non-alloc-class value: all releases are copy-releases, remove them.
        for (auto& r : release_list) {
          to_remove.insert(r.inst);
          if (stats) { ++stats->releases_eliminated; }
        }
      }
    }
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // Step 3 (M96): Move semantics — for remaining retained values, check if
  //               the original is not used after the copy (last-use pattern).
  //
  // Pattern: retain(a) → [no more uses of a except release(a)]
  //   → Remove retain(a) and release(a), ownership transfers to the copy.
  // ═══════════════════════════════════════════════════════════════════════════
  for (auto& [value_id, retain_list] : retains) {
    // Skip values already handled by M95 borrow elimination.
    bool all_removed = true;
    for (auto& r : retain_list) {
      if (!to_remove.count(r.inst)) {
        all_removed = false;
        break;
      }
    }
    if (all_removed) continue;

    auto release_it = releases.find(value_id);
    if (release_it == releases.end()) continue;
    auto& release_list = release_it->second;

    // Only handle simple case: exactly 1 remaining retain and 1+ releases.
    llvm::SmallVector<ARCInst*> remaining_retains;
    for (auto& r : retain_list) {
      if (!to_remove.count(r.inst)) {
        remaining_retains.push_back(&r);
      }
    }
    if (remaining_retains.size() != 1) continue;

    auto* retain = remaining_retains[0];

    // Don't apply move optimization to alloc_class values — the ownership
    // release must always remain.
    if (IsAllocClassProducer(function, value_id)) continue;

    // Find the release for this value that hasn't been marked for removal.
    ARCInst* matching_release = nullptr;
    for (auto& r : release_list) {
      if (!to_remove.count(r.inst)) {
        matching_release = &r;
        break;
      }
    }
    if (!matching_release) continue;

    // Check: is the value used between the retain and the release?
    // For simplicity, handle the single-block case where retain and release
    // are in the same block.
    if (retain->block_index != matching_release->block_index) continue;

    bool used_between = false;
    auto& bb = *function.blocks[retain->block_index];
    int start = retain->inst_index + 1;
    int end = matching_release->inst_index;

    for (int ii = start; ii < end; ++ii) {
      auto& inst = *bb.insts[ii];
      // Check if this instruction uses value_id (excluding ARC pair uses).
      auto check_operand = [&](TinySIL::SILValue op) {
        if (op.is_valid() && op.id == value_id) {
          auto kind = ClassifyUse(inst, value_id);
          if (kind != UseKind::ARCPair) {
            used_between = true;
          }
        }
      };

      for (int i = 0; i < inst.num_operands; ++i) {
        check_operand(inst.operands[i]);
      }
      for (const auto& op : inst.operand_list) {
        check_operand(op);
      }
      for (const auto& arg : inst.branch_args) {
        check_operand(arg);
      }
      if (used_between) break;
    }

    // Also check if value is used AFTER the release in any block.
    // For safety, only optimize if the value has no uses after the release
    // in the same block.
    if (!used_between) {
      for (int ii = matching_release->inst_index + 1;
           ii < static_cast<int>(bb.insts.size()); ++ii) {
        auto& inst = *bb.insts[ii];
        auto check_operand = [&](TinySIL::SILValue op) {
          if (op.is_valid() && op.id == value_id) {
            auto kind = ClassifyUse(inst, value_id);
            if (kind != UseKind::ARCPair) {
              used_between = true;
            }
          }
        };
        for (int i = 0; i < inst.num_operands; ++i) {
          check_operand(inst.operands[i]);
        }
        for (const auto& op : inst.operand_list) {
          check_operand(op);
        }
        for (const auto& arg : inst.branch_args) {
          check_operand(arg);
        }
        if (used_between) break;
      }
    }

    if (!used_between) {
      // Move optimization: remove the retain and release pair.
      // Ownership transfers to whatever consumed the retained copy.
      to_remove.insert(retain->inst);
      to_remove.insert(matching_release->inst);
      if (stats) { ++stats->moves_converted; }
    }
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // Step 4: Remove all marked instructions from their blocks.
  // ═══════════════════════════════════════════════════════════════════════════
  if (to_remove.empty()) return false;

  for (auto& bb : function.blocks) {
    llvm::SmallVector<std::unique_ptr<TinySIL::SILInstruction>> kept;
    for (auto& inst : bb->insts) {
      if (!to_remove.count(inst.get())) {
        kept.push_back(std::move(inst));
      }
    }
    bb->insts = std::move(kept);
  }
  return true;
}

}  // namespace TinySwift::TinySILOptimizer
