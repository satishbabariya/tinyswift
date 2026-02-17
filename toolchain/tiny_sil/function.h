// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_FUNCTION_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_FUNCTION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "llvm/ADT/SmallVector.h"
#include "toolchain/tiny_sil/basic_block.h"
#include "toolchain/tiny_sil/sil_type.h"

namespace TinySwift::TinySIL {

// A SIL function.
struct SILFunction {
  std::string name;
  SILFunctionType type;
  Ownership convention = Ownership::None;
  bool is_thunk = false;
  bool is_declaration = false;  // True if no body.

  // The function's basic blocks.
  llvm::SmallVector<std::unique_ptr<SILBasicBlock>> blocks;

  // Next value ID to assign.
  int32_t next_value_id = 0;

  // Allocates a new value ID.
  auto allocateValueId() -> int32_t { return next_value_id++; }

  // Creates and adds a new basic block.
  auto createBasicBlock() -> SILBasicBlock* {
    auto bb = std::make_unique<SILBasicBlock>();
    bb->id = static_cast<int32_t>(blocks.size());
    auto* ptr = bb.get();
    blocks.push_back(std::move(bb));
    return ptr;
  }

  // Returns the entry block.
  auto getEntryBlock() const -> SILBasicBlock* {
    if (blocks.empty()) {
      return nullptr;
    }
    return blocks[0].get();
  }

  // Returns true if this function has a body.
  auto hasBody() const -> bool { return !blocks.empty(); }

  // SemIR function ID for cross-referencing.
  int32_t sem_ir_function_id = -1;
};

}  // namespace TinySwift::TinySIL

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_FUNCTION_H_
