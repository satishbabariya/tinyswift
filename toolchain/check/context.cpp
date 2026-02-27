// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/context.h"

#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

Context::Context(Unit& unit, const Parse::TreeAndSubtrees& tree_and_subtrees,
                 Diagnostics::Consumer& consumer)
    : sem_ir_(unit.sem_ir),
      tree_and_subtrees_(&tree_and_subtrees),
      consumer_(&consumer),
      emitter_(&consumer, &tokens()) {}

auto Context::SetCurrentTreeAndSubtrees(
    const Parse::TreeAndSubtrees& tree_and_subtrees) -> void {
  tree_and_subtrees_ = &tree_and_subtrees;
  // Re-create the emitter with the new file's tokens so diagnostics report
  // correct locations.
  emitter_ = TokenEmitter(consumer_, &tokens());
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

auto Context::SwitchInstBlock(SemIR::InstBlockId new_block_id) -> void {
  if (!inst_block_stack_.empty()) {
    auto entry = std::move(inst_block_stack_.back());
    inst_block_stack_.pop_back();
    inst_blocks().ReplacePlaceholder(
        entry.id, llvm::ArrayRef<SemIR::InstId>(entry.insts));
  }
  inst_block_stack_.push_back({new_block_id, {}});
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

auto Context::PushInstBlockWithId(SemIR::InstBlockId block_id) -> void {
  inst_block_stack_.push_back({block_id, {}});
}

auto Context::PushLoopContext(SemIR::InstBlockId break_id,
                              SemIR::InstBlockId continue_id) -> void {
  loop_stack_.push_back({break_id, continue_id});
}

auto Context::PopLoopContext() -> void {
  if (!loop_stack_.empty()) {
    loop_stack_.pop_back();
  }
}

auto Context::CurrentLoop() const -> const LoopContext* {
  if (loop_stack_.empty()) return nullptr;
  return &loop_stack_.back();
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
      auto inst_id = found->second;
      // M74: Enforce private access — skip names marked private if the
      // current scope is not the declaring scope or a child of it.
      auto access = GetAccessLevel(inst_id);
      if (access == SemIR::AccessLevel::Private) {
        auto decl_scope_it = private_decl_scope_map_.find(inst_id.index);
        if (decl_scope_it != private_decl_scope_map_.end()) {
          auto decl_scope = decl_scope_it->second;
          // Check if the current scope matches or is a child of the declaring scope.
          bool accessible = false;
          for (auto sit = scope_stack_.rbegin(); sit != scope_stack_.rend(); ++sit) {
            if (sit->scope_id == decl_scope) {
              accessible = true;
              break;
            }
          }
          if (!accessible) {
            continue;  // Skip this private name, keep searching outer scopes.
          }
        }
      }
      return inst_id;
    }
  }
  return SemIR::InstId::None;
}

auto Context::IsCurrentBlockTerminated() -> bool {
  if (inst_block_stack_.empty()) return false;
  const auto& block_insts = inst_block_stack_.back().insts;
  if (block_insts.empty()) return false;
  auto last_id = block_insts.back();
  auto kind = insts().Get(last_id).kind();
  return kind == SemIR::InstKind::Return ||
         kind == SemIR::InstKind::ReturnExpr ||
         kind == SemIR::InstKind::Branch ||
         kind == SemIR::InstKind::BranchIf ||
         kind == SemIR::InstKind::ThrowValue;
}

auto Context::AddNameToScope(SemIR::NameId name_id, SemIR::InstId inst_id)
    -> void {
  if (!scope_stack_.empty()) {
    scope_stack_.back().names.insert_or_assign(name_id.index, inst_id);
    // Also persist in the NameScope so member access can find names after
    // the scope is popped.
    auto scope_id = scope_stack_.back().scope_id;
    if (scope_id.has_value()) {
      name_scopes().Get(scope_id).names.insert_or_assign(name_id.index,
                                                         inst_id);
    }
  }
}

auto Context::AddBodyBlock(SemIR::InstBlockId block_id) -> void {
  if (current_function_id_.has_value()) {
    functions().Get(current_function_id_).body_block_ids.push_back(block_id);
  }
}

auto Context::CurrentScopeId() const -> SemIR::NameScopeId {
  if (scope_stack_.empty()) {
    return SemIR::NameScopeId::None;
  }
  return scope_stack_.back().scope_id;
}

auto Context::GetTypeName(SemIR::TypeId type_id) -> std::string {
  if (!type_id.has_value() || type_id == SemIR::ErrorInst::TypeId) {
    return "<error>";
  }
  if (!type_id.is_concrete()) {
    return "<unknown>";
  }
  auto type_inst_id = types().GetTypeInstId(type_id);
  if (!type_inst_id.has_value()) {
    return "<unknown>";
  }
  auto type_inst = insts().Get(type_inst_id);
  switch (type_inst.kind()) {
    case SemIR::InstKind::BoolType:
      return "Bool";
    case SemIR::InstKind::IntType:
    case SemIR::InstKind::IntLiteralType:
      return "Int";
    case SemIR::InstKind::StringType:
      return "String";
    case SemIR::InstKind::FloatType:
      return "Float";
    case SemIR::InstKind::DoubleType:
      return "Double";
    case SemIR::InstKind::StructType: {
      auto struct_type = type_inst.As<SemIR::StructType>();
      auto& scope = name_scopes().Get(struct_type.name_scope_id);
      if (scope.name_id.AsIdentifierId().has_value()) {
        return std::string(identifiers().Get(scope.name_id.AsIdentifierId()));
      }
      return "<struct>";
    }
    case SemIR::InstKind::ClassType: {
      auto class_type = type_inst.As<SemIR::ClassType>();
      auto& scope = name_scopes().Get(class_type.name_scope_id);
      if (scope.name_id.AsIdentifierId().has_value()) {
        return std::string(identifiers().Get(scope.name_id.AsIdentifierId()));
      }
      return "<class>";
    }
    case SemIR::InstKind::OptionalType:
      return "Optional";
    case SemIR::InstKind::FunctionType:
      return "Function";
    case SemIR::InstKind::PointerType:
      return "Pointer";
    default:
      return "<type>";
  }
}

// M88: Returns the NameScope for a built-in type, creating one if needed.
auto Context::GetOrCreateBuiltinTypeScope(SemIR::InstId type_inst_id)
    -> SemIR::NameScopeId {
  auto it = builtin_type_scopes_.find(type_inst_id.index);
  if (it != builtin_type_scopes_.end()) {
    return it->second;
  }
  auto scope_id = name_scopes().Add(type_inst_id, SemIR::NameId::None,
                                     SemIR::NameScopeId::None);
  builtin_type_scopes_.insert_or_assign(type_inst_id.index, scope_id);
  return scope_id;
}

// M88: Returns the NameScope for a built-in type, or NameScopeId::None.
auto Context::GetBuiltinTypeScope(SemIR::InstId type_inst_id)
    -> SemIR::NameScopeId {
  auto it = builtin_type_scopes_.find(type_inst_id.index);
  if (it != builtin_type_scopes_.end()) {
    return it->second;
  }
  return SemIR::NameScopeId::None;
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
  // M75: Unsafe pointer types — lower to LLVM opaque pointer (ptr).
  if (name == "UnsafeRawPointer" || name == "UnsafePointer" ||
      name == "UnsafeMutableRawPointer" || name == "UnsafeMutablePointer" ||
      name == "OpaquePointer") {
    // Reuse PointerType with Int as pointee — lowers to LLVM ptr.
    auto int_type_inst_id =
        types().GetTypeInstId(GetBuiltinType("Int"));
    auto inst_id = AddInstInNoBlock(SemIR::LocIdAndInst::NoLoc(
        SemIR::PointerType{.type_id = SemIR::TypeType::TypeId,
                           .pointee_id = int_type_inst_id}));
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(inst_id));
  }
  // Unknown type.
  return SemIR::ErrorInst::TypeId;
}

// M78: Check if a TypeId is a class reference type.
auto Context::IsReferenceType(SemIR::TypeId type_id) -> bool {
  if (!type_id.has_value() || !type_id.is_concrete()) return false;
  auto type_inst_id = types().GetTypeInstId(type_id);
  if (!type_inst_id.has_value()) return false;
  auto type_inst = insts().Get(type_inst_id);
  return type_inst.Is<SemIR::ClassType>();
}

// M78: Emit Release instructions for all tracked class-typed locals in
// the current scope (LIFO order), then pop the scope.
auto Context::PopCleanupScope(Parse::NodeId loc) -> void {
  if (class_cleanup_stack_.empty()) return;
  auto scope = std::move(class_cleanup_stack_.back());
  class_cleanup_stack_.pop_back();
  // Skip emitting releases if the current block is already terminated
  // (e.g., by a return statement that already emitted its own releases
  // via EmitCleanupReleases).
  if (IsCurrentBlockTerminated()) return;
  // Emit releases in reverse (LIFO) order.
  for (int i = static_cast<int>(scope.size()) - 1; i >= 0; --i) {
    auto& cleanup = scope[i];
    AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(loc),
        SemIR::Release{.type_id = SemIR::ErrorInst::TypeId,
                       .value_id = cleanup.var_id,
                       .deinit_id = cleanup.deinit_id}));
  }
}

// M78: Emit Release for all current scope locals without popping.
auto Context::EmitCleanupReleases(Parse::NodeId loc) -> void {
  if (class_cleanup_stack_.empty()) return;
  auto& scope = class_cleanup_stack_.back();
  for (int i = static_cast<int>(scope.size()) - 1; i >= 0; --i) {
    auto& cleanup = scope[i];
    AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(loc),
        SemIR::Release{.type_id = SemIR::ErrorInst::TypeId,
                       .value_id = cleanup.var_id,
                       .deinit_id = cleanup.deinit_id}));
  }
}

// M78/M79: Look up __deinit in a class's NameScope.
auto Context::GetClassDeinitId(SemIR::TypeId type_id) -> SemIR::InstId {
  if (!type_id.has_value() || !type_id.is_concrete()) return SemIR::InstId::None;
  auto type_inst_id = types().GetTypeInstId(type_id);
  if (!type_inst_id.has_value()) return SemIR::InstId::None;
  auto type_inst = insts().Get(type_inst_id);
  auto ct = type_inst.TryAs<SemIR::ClassType>();
  if (!ct) return SemIR::InstId::None;
  auto& scope = name_scopes().Get(ct->name_scope_id);
  auto deinit_ident_id = identifiers().Lookup("__deinit");
  if (!deinit_ident_id.has_value()) return SemIR::InstId::None;
  auto deinit_name_id = SemIR::NameId::ForIdentifier(deinit_ident_id);
  auto it = scope.names.find(deinit_name_id.index);
  if (it != scope.names.end()) return it->second;
  return SemIR::InstId::None;
}

auto Context::AnalyzeCycleCapability(SemIR::InstId class_type_id,
                                     SemIR::NameScopeId scope_id) -> void {
  // M97: Check if any stored field in this class has a class type.
  // If so, the class is cycle-capable (it could form reference cycles).
  if (!scope_id.has_value()) return;

  auto& scope = name_scopes().Get(scope_id);
  for (auto& [name_idx, inst_id] : scope.names) {
    auto inst = insts().Get(inst_id);
    if (auto sf = inst.TryAs<SemIR::StructField>()) {
      // Check if the field's type is a class type.
      auto field_type_id = sf->type_id;
      if (IsReferenceType(field_type_id)) {
        // This class has a class-typed field — mark as cycle-capable.
        auto class_type_id_as_type =
            types().GetTypeIdForTypeInstId(class_type_id);
        if (class_type_id_as_type.has_value()) {
          sem_ir().MarkCycleCapableType(class_type_id_as_type);
        }
        return;
      }
      // Also check Optional<ClassType> — optional class references can cycle.
      if (field_type_id.has_value() && field_type_id.is_concrete()) {
        auto ft_inst_id = types().GetTypeInstId(field_type_id);
        if (ft_inst_id.has_value()) {
          auto ft_inst = insts().Get(ft_inst_id);
          if (auto opt_type = ft_inst.TryAs<SemIR::OptionalType>()) {
            // inner_type_id is a TypeInstId (subclass of InstId).
            // Get the inst at that ID and check if it's a ClassType.
            auto inner_inst_id = SemIR::InstId(opt_type->inner_type_id.index);
            if (inner_inst_id.has_value()) {
              auto inner_inst = insts().Get(inner_inst_id);
              if (inner_inst.Is<SemIR::ClassType>()) {
                auto class_type_id_as_type =
                    types().GetTypeIdForTypeInstId(class_type_id);
                if (class_type_id_as_type.has_value()) {
                  sem_ir().MarkCycleCapableType(class_type_id_as_type);
                }
                return;
              }
            }
          }
        }
      }
    }
  }
}

}  // namespace TinySwift::Check
