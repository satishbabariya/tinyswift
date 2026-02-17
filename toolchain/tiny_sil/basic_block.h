// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_BASIC_BLOCK_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_BASIC_BLOCK_H_

#include <cstdint>
#include <memory>

#include "llvm/ADT/SmallVector.h"
#include "toolchain/tiny_sil/instruction.h"
#include "toolchain/tiny_sil/sil_value.h"

namespace TinySwift::TinySIL {

// A basic block in the SIL CFG. Block arguments replace phi nodes.
struct SILBasicBlock {
  int32_t id = -1;

  // Block arguments (replace phi nodes from SSA).
  llvm::SmallVector<SILValue> args;

  // Instructions in this block.
  llvm::SmallVector<std::unique_ptr<SILInstruction>> insts;

  // Adds an instruction to this block.
  auto addInst(std::unique_ptr<SILInstruction> inst) -> SILInstruction* {
    auto* ptr = inst.get();
    insts.push_back(std::move(inst));
    return ptr;
  }

  // Returns the terminator instruction, or nullptr if none.
  auto getTerminator() const -> SILInstruction* {
    if (insts.empty()) {
      return nullptr;
    }
    auto* last = insts.back().get();
    switch (last->kind) {
      case SILInstKind::Branch:
      case SILInstKind::CondBranch:
      case SILInstKind::SwitchEnum:
      case SILInstKind::ReturnInst:
      case SILInstKind::Unreachable:
        return last;
      default:
        return nullptr;
    }
  }

  auto is_terminated() const -> bool { return getTerminator() != nullptr; }
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_BASIC_BLOCK_H_
