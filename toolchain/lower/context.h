// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_

#include <memory>

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "toolchain/lex/tokenized_buffer.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"

namespace TinySwift::Lower {

// Central state holder for lowering SemIR to LLVM IR.
class Context {
 public:
  explicit Context(llvm::Module& module, llvm::LLVMContext& llvm_context,
                   const SemIR::File& sem_ir,
                   const Lex::TokenizedBuffer* tokens = nullptr,
                   bool want_debug_info = false);

  auto llvm_context() -> llvm::LLVMContext& { return llvm_context_; }
  auto module() -> llvm::Module& { return module_; }
  auto builder() -> llvm::IRBuilder<>& { return builder_; }
  auto sem_ir() const -> const SemIR::File& { return sem_ir_; }

  // M119: Debug info support.
  auto debug_enabled() const -> bool { return debug_enabled_; }
  auto di_builder() -> llvm::DIBuilder* { return di_builder_.get(); }
  auto di_file() -> llvm::DIFile* { return di_file_; }
  auto di_compile_unit() -> llvm::DICompileUnit* { return di_compile_unit_; }
  auto SetDebugLoc(SemIR::LocId loc_id) -> void;
  auto ClearDebugLoc() -> void;
  auto SetCurrentScope(llvm::DIScope* scope) -> void { current_scope_ = scope; }
  auto current_scope() -> llvm::DIScope* { return current_scope_; }
  auto ResolveLocToLineCol(SemIR::LocId loc_id,
                           unsigned& line, unsigned& col) -> bool;

  // M119: Create a DISubroutineType for a function.
  auto CreateFunctionDIType() -> llvm::DISubroutineType*;

  // M120: Debug type mapping.
  auto GetOrCreateDIType(SemIR::TypeId type_id) -> llvm::DIType*;

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

  // M119: Debug info state.
  std::unique_ptr<llvm::DIBuilder> di_builder_;
  llvm::DICompileUnit* di_compile_unit_ = nullptr;
  llvm::DIFile* di_file_ = nullptr;
  llvm::DIScope* current_scope_ = nullptr;
  const Lex::TokenizedBuffer* tokens_ = nullptr;
  bool debug_enabled_ = false;

  // M120: Cached debug type mappings.
  llvm::DenseMap<int32_t, llvm::DIType*> di_type_cache_;
};

// Forward declarations for handler functions.
auto LowerType(Context& context, SemIR::TypeId type_id) -> llvm::Type*;
auto LowerInst(Context& context, SemIR::InstId inst_id) -> void;

// M78: Returns the LLVM struct type for a class's fields layout.
auto GetClassFieldsType(Context& context, SemIR::TypeId class_type_id)
    -> llvm::StructType*;

}  // namespace TinySwift::Lower

#endif  // TINYSWIFT_TOOLCHAIN_LOWER_CONTEXT_H_
