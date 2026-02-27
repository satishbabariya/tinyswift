// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_CONTEXT_H_
#define TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_CONTEXT_H_

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/sem_ir/file.h"
#include "toolchain/sem_ir/ids.h"
#include "toolchain/tiny_sil/module.h"
#include "toolchain/tiny_sil_gen/managed_value.h"

namespace TinySwift::TinySILGen {

// Context for SILGen — tracks state while generating TinySIL from SemIR.
class Context {
 public:
  explicit Context(const SemIR::File& sem_ir, TinySIL::SILModule& sil_module);

  auto sem_ir() const -> const SemIR::File& { return sem_ir_; }
  auto sil_module() -> TinySIL::SILModule& { return sil_module_; }

  // --- Value mapping ---
  // Maps a SemIR InstId to its corresponding SIL value.
  auto SetValue(SemIR::InstId inst_id, TinySIL::SILValue value) -> void;
  auto GetValue(SemIR::InstId inst_id) const -> TinySIL::SILValue;
  auto HasValue(SemIR::InstId inst_id) const -> bool;

  // --- Block mapping ---
  // Maps a SemIR block (InstBlockId) to a SIL basic block index.
  auto SetBlock(SemIR::InstBlockId block_id, int32_t sil_block_id) -> void;
  auto GetBlock(SemIR::InstBlockId block_id) const -> int32_t;
  auto HasBlock(SemIR::InstBlockId block_id) const -> bool;

  // --- Name resolution ---
  auto GetNameString(SemIR::NameId name_id) const -> std::string;
  auto GetFunctionName(const SemIR::Function& function) const -> std::string;

  // --- Type mapping ---
  // Creates a SILType from a SemIR TypeId.
  auto GetSILType(SemIR::TypeId type_id) const -> TinySIL::SILType;

  // --- Current function context ---
  auto current_function() -> TinySIL::SILFunction* { return current_function_; }
  auto set_current_function(TinySIL::SILFunction* fn) -> void {
    current_function_ = fn;
  }

  auto current_block() -> TinySIL::SILBasicBlock* { return current_block_; }
  auto set_current_block(TinySIL::SILBasicBlock* bb) -> void {
    current_block_ = bb;
  }

  // Allocate a new value ID in the current function.
  auto allocateValueId() -> int32_t {
    return current_function_->allocateValueId();
  }

  // Emit an instruction to the current block and return the result value.
  auto emit(std::unique_ptr<TinySIL::SILInstruction> inst)
      -> TinySIL::SILValue;

  // M119: Track current source location for debug info threading.
  auto set_current_loc_id(SemIR::LocId loc_id) -> void {
    current_loc_id_ = loc_id;
  }
  auto current_loc_id() const -> SemIR::LocId { return current_loc_id_; }

  // Clear per-function state for the next function.
  auto clearFunctionState() -> void;

  // --- Array var tracking ---
  // VarStorage InstIds that are array-backed (initialized via ArrayLiteralInit).
  // NameRef → these VarStorages should NOT emit a Load (the ALI is used directly).
  auto MarkArrayVar(int32_t var_storage_raw_id) -> void {
    array_var_ids_.insert(var_storage_raw_id);
  }
  auto IsArrayVar(int32_t var_storage_raw_id) const -> bool {
    return array_var_ids_.count(var_storage_raw_id) > 0;
  }

 private:
  const SemIR::File& sem_ir_;
  TinySIL::SILModule& sil_module_;

  // Per-function state.
  TinySIL::SILFunction* current_function_ = nullptr;
  TinySIL::SILBasicBlock* current_block_ = nullptr;
  llvm::DenseMap<int32_t, TinySIL::SILValue> value_map_;
  llvm::DenseMap<int32_t, int32_t> block_map_;
  // VarStorage InstIds backed by an ArrayLiteralInit (no Store needed).
  llvm::DenseSet<int32_t> array_var_ids_;
  // M119: Current source location for debug info.
  SemIR::LocId current_loc_id_ = SemIR::LocId::None;
};

}  // namespace TinySwift::TinySILGen

#endif  // TINYSWIFT_TOOLCHAIN_TINY_SIL_GEN_CONTEXT_H_
