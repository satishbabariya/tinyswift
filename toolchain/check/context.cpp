// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/context.h"

#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

Context::Context(Unit& unit, const Parse::TreeAndSubtrees& tree_and_subtrees,
                 Diagnostics::Consumer& consumer)
    : sem_ir_(unit.sem_ir),
      tree_and_subtrees_(&tree_and_subtrees) {
  (void)consumer;  // Will be used when diagnostics are emitted.
}

auto Context::AddInstInNoBlock(SemIR::LocIdAndInst loc_and_inst)
    -> SemIR::InstId {
  return insts().AddInNoBlock(loc_and_inst);
}

auto Context::AddInst(SemIR::LocIdAndInst loc_and_inst) -> SemIR::InstId {
  auto inst_id = insts().AddInNoBlock(loc_and_inst);
  if (!inst_block_stack_.empty()) {
    inst_block_stack_.back().insts.push_back(inst_id);
  }
  return inst_id;
}

auto Context::PushInstBlock() -> SemIR::InstBlockId {
  auto placeholder_id = inst_blocks().AddPlaceholder();
  inst_block_stack_.push_back({placeholder_id, {}});
  return placeholder_id;
}

auto Context::PopInstBlock() -> SemIR::InstBlockId {
  auto entry = std::move(inst_block_stack_.back());
  inst_block_stack_.pop_back();
  inst_blocks().ReplacePlaceholder(
      entry.id, llvm::ArrayRef<SemIR::InstId>(entry.insts));
  return entry.id;
}

auto Context::CurrentInstBlockId() const -> SemIR::InstBlockId {
  if (inst_block_stack_.empty()) {
    return SemIR::InstBlockId::None;
  }
  return inst_block_stack_.back().id;
}

auto Context::AddInstToBlock(SemIR::InstBlockId block_id,
                             SemIR::InstId inst_id) -> void {
  // Find the block in the stack and add to it.
  for (auto& entry : inst_block_stack_) {
    if (entry.id == block_id) {
      entry.insts.push_back(inst_id);
      return;
    }
  }
}

auto Context::PushScope(SemIR::NameScopeId scope_id) -> void {
  scope_stack_.emplace_back(scope_id);
}

auto Context::PopScope() -> void { scope_stack_.pop_back(); }

auto Context::LookupName(SemIR::NameId name_id) -> SemIR::InstId {
  // Walk scopes from innermost to outermost.
  for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
    auto found = it->names.find(name_id.index);
    if (found != it->names.end()) {
      return found->second;
    }
  }
  return SemIR::InstId::None;
}

auto Context::AddNameToScope(SemIR::NameId name_id, SemIR::InstId inst_id)
    -> void {
  if (!scope_stack_.empty()) {
    scope_stack_.back().names.insert_or_assign(name_id.index, inst_id);
  }
}

auto Context::CurrentScopeId() const -> SemIR::NameScopeId {
  if (scope_stack_.empty()) {
    return SemIR::NameScopeId::None;
  }
  return scope_stack_.back().scope_id;
}

auto Context::GetBuiltinType(llvm::StringRef name) -> SemIR::TypeId {
  if (name == "Bool") {
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));
  }
  if (name == "Int") {
    // Use IntLiteralType as a stand-in for Swift's Int type.
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(
            SemIR::IntLiteralType::TypeInstId));
  }
  if (name == "String") {
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::StringType::TypeInstId));
  }
  if (name == "Float") {
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::FloatType::TypeInstId));
  }
  if (name == "Double") {
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(SemIR::DoubleType::TypeInstId));
  }
  if (name == "Void") {
    // Void is represented as the empty tuple type, but for now use TypeType.
    return SemIR::TypeType::TypeId;
  }
  // Unknown type.
  return SemIR::ErrorInst::TypeId;
}

}  // namespace TinySwift::Check
