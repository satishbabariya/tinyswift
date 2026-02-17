// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lower/context.h"

#include "common/check.h"

namespace TinySwift::Lower {

Context::Context(llvm::Module& module, llvm::LLVMContext& llvm_context,
                 const SemIR::File& sem_ir)
    : module_(module),
      llvm_context_(llvm_context),
      sem_ir_(sem_ir),
      builder_(llvm_context) {}

auto Context::GetType(SemIR::TypeId type_id) -> llvm::Type* {
  auto it = type_cache_.find(type_id.index);
  if (it != type_cache_.end()) {
    return it->second;
  }
  auto* result = LowerType(*this, type_id);
  type_cache_[type_id.index] = result;
  return result;
}

auto Context::SetLocal(SemIR::InstId inst_id, llvm::Value* value) -> void {
  auto result = locals_.insert({inst_id.index, value});
  TINYSWIFT_CHECK(result.second || result.first->second == value,
               "Duplicate local for inst {0}", inst_id);
}

auto Context::GetLocal(SemIR::InstId inst_id) -> llvm::Value* {
  auto it = locals_.find(inst_id.index);
  TINYSWIFT_CHECK(it != locals_.end(), "Missing local for inst {0}", inst_id);
  return it->second;
}

auto Context::TryGetLocal(SemIR::InstId inst_id) -> llvm::Value* {
  auto it = locals_.find(inst_id.index);
  if (it != locals_.end()) {
    return it->second;
  }
  return nullptr;
}

auto Context::GetFunction(SemIR::FunctionId function_id) -> llvm::Function* {
  auto it = functions_.find(function_id.index);
  TINYSWIFT_CHECK(it != functions_.end(), "Missing function for {0}",
               function_id);
  return it->second;
}

auto Context::SetFunction(SemIR::FunctionId function_id,
                          llvm::Function* function) -> void {
  functions_[function_id.index] = function;
}

auto Context::GetBlock(SemIR::InstBlockId block_id) -> llvm::BasicBlock* {
  auto it = blocks_.find(block_id.index);
  TINYSWIFT_CHECK(it != blocks_.end(), "Missing block for {0}", block_id);
  return it->second;
}

auto Context::GetOrCreateBlock(SemIR::InstBlockId block_id,
                               llvm::Function* function) -> llvm::BasicBlock* {
  auto it = blocks_.find(block_id.index);
  if (it != blocks_.end()) {
    return it->second;
  }
  auto* block = llvm::BasicBlock::Create(llvm_context_, "", function);
  blocks_[block_id.index] = block;
  return block;
}

auto Context::SetBlockArg(SemIR::InstBlockId block_id,
                          llvm::Value* value) -> void {
  block_args_[block_id.index] = value;
}

auto Context::GetBlockArg(SemIR::InstBlockId block_id) -> llvm::Value* {
  auto it = block_args_.find(block_id.index);
  TINYSWIFT_CHECK(it != block_args_.end(), "Missing block arg for {0}",
               block_id);
  return it->second;
}

}  // namespace TinySwift::Lower
