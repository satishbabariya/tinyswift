// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/tiny_sil_gen/context.h"

namespace TinySwift::TinySILGen {

Context::Context(const SemIR::File& sem_ir, TinySIL::SILModule& sil_module)
    : sem_ir_(sem_ir), sil_module_(sil_module) {}

auto Context::SetValue(SemIR::InstId inst_id, TinySIL::SILValue value)
    -> void {
  value_map_[inst_id.index] = value;
}

auto Context::GetValue(SemIR::InstId inst_id) const -> TinySIL::SILValue {
  auto it = value_map_.find(inst_id.index);
  if (it != value_map_.end()) {
    return it->second;
  }
  return TinySIL::SILValue::None();
}

auto Context::HasValue(SemIR::InstId inst_id) const -> bool {
  return value_map_.count(inst_id.index) > 0;
}

auto Context::SetBlock(SemIR::InstBlockId block_id, int32_t sil_block_id)
    -> void {
  block_map_[block_id.index] = sil_block_id;
}

auto Context::GetBlock(SemIR::InstBlockId block_id) const -> int32_t {
  auto it = block_map_.find(block_id.index);
  if (it != block_map_.end()) {
    return it->second;
  }
  return -1;
}

auto Context::HasBlock(SemIR::InstBlockId block_id) const -> bool {
  return block_map_.count(block_id.index) > 0;
}

auto Context::GetNameString(SemIR::NameId name_id) const -> std::string {
  auto identifier_id = name_id.AsIdentifierId();
  if (identifier_id.has_value()) {
    return std::string(sem_ir_.identifiers().Get(identifier_id));
  }
  return "<unnamed>";
}

auto Context::GetFunctionName(const SemIR::Function& function) const
    -> std::string {
  return GetNameString(function.name_id);
}

auto Context::GetSILType(SemIR::TypeId type_id) const -> TinySIL::SILType {
  if (!type_id.has_value()) {
    return TinySIL::SILType{};
  }
  // Use the type_id index as the SIL type index for cross-referencing.
  return TinySIL::SILType{.type_index = type_id.index, .is_address = false};
}

auto Context::emit(std::unique_ptr<TinySIL::SILInstruction> inst)
    -> TinySIL::SILValue {
  auto result = inst->result;
  current_block_->addInst(std::move(inst));
  return result;
}

auto Context::clearFunctionState() -> void {
  current_function_ = nullptr;
  current_block_ = nullptr;
  value_map_.clear();
  block_map_.clear();
}

}  // namespace TinySwift::TinySILGen
