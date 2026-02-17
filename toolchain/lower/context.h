// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"

namespace TinySwift::Lower {

// Central state holder for lowering SemIR to LLVM IR.
class Context {
 public:
  explicit Context(llvm::Module& module, llvm::LLVMContext& llvm_context,
                   const SemIR::File& sem_ir);

  auto llvm_context() -> llvm::LLVMContext& { return llvm_context_; }
  auto module() -> llvm::Module& { return module_; }
  auto builder() -> llvm::IRBuilder<>& { return builder_; }
  auto sem_ir() const -> const SemIR::File& { return sem_ir_; }

  // Type lowering: maps SemIR TypeId to LLVM type, with caching.
  auto GetType(SemIR::TypeId type_id) -> llvm::Type*;

  // Value mapping: associates a SemIR instruction with its LLVM value.
  auto SetLocal(SemIR::InstId inst_id, llvm::Value* value) -> void;
  auto GetLocal(SemIR::InstId inst_id) -> llvm::Value*;
  auto TryGetLocal(SemIR::InstId inst_id) -> llvm::Value*;

  // Function mapping: get the LLVM function for a SemIR function.
  auto GetFunction(SemIR::FunctionId function_id) -> llvm::Function*;
  auto SetFunction(SemIR::FunctionId function_id,
                   llvm::Function* function) -> void;

  // Block mapping: get or create LLVM basic blocks for SemIR inst blocks.
  auto GetBlock(SemIR::InstBlockId block_id) -> llvm::BasicBlock*;
  auto GetOrCreateBlock(SemIR::InstBlockId block_id,
                        llvm::Function* function) -> llvm::BasicBlock*;

  // Block argument support for BranchWithArg/BlockArg.
  auto SetBlockArg(SemIR::InstBlockId block_id, llvm::Value* value) -> void;
  auto GetBlockArg(SemIR::InstBlockId block_id) -> llvm::Value*;

 private:
  llvm::Module& module_;
  llvm::LLVMContext& llvm_context_;
  const SemIR::File& sem_ir_;
  llvm::IRBuilder<> builder_;

  // Cached type mappings.
  llvm::DenseMap<int32_t, llvm::Type*> type_cache_;

  // InstId -> llvm::Value* for lowered instructions.
  llvm::DenseMap<int32_t, llvm::Value*> locals_;

  // FunctionId -> llvm::Function* for declared functions.
  llvm::DenseMap<int32_t, llvm::Function*> functions_;

  // InstBlockId -> llvm::BasicBlock* for control flow blocks.
  llvm::DenseMap<int32_t, llvm::BasicBlock*> blocks_;

  // InstBlockId -> llvm::Value* for block arguments (BranchWithArg/BlockArg).
  llvm::DenseMap<int32_t, llvm::Value*> block_args_;
};

// Forward declarations for handler functions.
auto LowerType(Context& context, SemIR::TypeId type_id) -> llvm::Type*;
auto LowerInst(Context& context, SemIR::InstId inst_id) -> void;

}  // namespace TinySwift::Lower

#endif  // TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_
