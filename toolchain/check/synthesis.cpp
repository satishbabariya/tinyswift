// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/synthesis.h"

#include <deque>

#include "toolchain/check/context.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

// Persistent storage for synthesized name strings. Using deque ensures
// push_back never invalidates prior StringRefs.
static auto SynthNameStorage() -> std::deque<std::string>& {
  static auto* storage = new std::deque<std::string>();
  return *storage;
}

// Helper: create a SemIR::NameId from a string, storing it persistently.
static auto MakeName(Context& context, llvm::StringRef text) -> SemIR::NameId {
  SynthNameStorage().push_back(std::string(text));
  auto ident_id =
      context.identifiers().Add(SynthNameStorage().back());
  return SemIR::NameId::ForIdentifier(ident_id);
}

// Helper: get the SemIR::TypeId for an InstId that represents a type.
static auto TypeIdFor(SemIR::InstId type_inst_id) -> SemIR::TypeId {
  return SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(type_inst_id));
}

// Helper: check if a name already exists in a scope.
static auto IsNameInScope(Context& context, SemIR::NameScopeId scope_id,
                          llvm::StringRef name) -> bool {
  auto ident_id = context.identifiers().Add(name);
  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto& scope = context.name_scopes().Get(scope_id);
  return scope.names.find(name_id.index) != scope.names.end();
}

// Helper: get type name string from enclosing type inst.
static auto GetTypeNameStr(Context& context, SemIR::InstId type_inst_id)
    -> llvm::StringRef {
  auto type_inst = context.insts().Get(type_inst_id);
  if (auto st = type_inst.TryAs<SemIR::StructType>()) {
    auto& scope = context.name_scopes().Get(st->name_scope_id);
    auto ident = scope.name_id.AsIdentifierId();
    if (ident.has_value()) return context.identifiers().Get(ident);
  } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
    auto& scope = context.name_scopes().Get(ct->name_scope_id);
    auto ident = scope.name_id.AsIdentifierId();
    if (ident.has_value()) return context.identifiers().Get(ident);
  } else if (auto et = type_inst.TryAs<SemIR::EnumDecl>()) {
    auto& scope = context.name_scopes().Get(et->name_scope_id);
    auto ident = scope.name_id.AsIdentifierId();
    if (ident.has_value()) return context.identifiers().Get(ident);
  }
  return "";
}

// Helper: emit field equality comparison for a single field.
// Returns InstId of a Bool-typed comparison result.
static auto EmitFieldEq(Context& context, SemIR::InstId lhs_param,
                        SemIR::InstId rhs_param, int32_t field_idx,
                        SemIR::TypeId field_type_id) -> SemIR::InstId {
  auto bool_type_id = context.GetBuiltinType("Bool");
  auto lhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = lhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));
  auto rhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = rhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));

  auto int_type = context.GetBuiltinType("Int");
  auto string_type = context.GetBuiltinType("String");
  auto float_type = context.GetBuiltinType("Float");

  if (field_type_id == int_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntEq{.type_id = bool_type_id,
                     .lhs_id = lhs_field,
                     .rhs_id = rhs_field}));
  }
  if (field_type_id == string_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::StringEq{.type_id = bool_type_id,
                        .lhs_id = lhs_field,
                        .rhs_id = rhs_field}));
  }
  if (field_type_id == float_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FloatEq{.type_id = bool_type_id,
                       .lhs_id = lhs_field,
                       .rhs_id = rhs_field}));
  }
  if (field_type_id == bool_type_id) {
    // Bool equality: use IntEq on the i1 values underneath.
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntEq{.type_id = bool_type_id,
                     .lhs_id = lhs_field,
                     .rhs_id = rhs_field}));
  }
  // Fallback for user-defined types: assume IntEq on their representation.
  return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::IntEq{.type_id = bool_type_id,
                   .lhs_id = lhs_field,
                   .rhs_id = rhs_field}));
}

// Helper: emit field less-than comparison for Comparable.
static auto EmitFieldLess(Context& context, SemIR::InstId lhs_param,
                          SemIR::InstId rhs_param, int32_t field_idx,
                          SemIR::TypeId field_type_id) -> SemIR::InstId {
  auto bool_type_id = context.GetBuiltinType("Bool");
  auto lhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = lhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));
  auto rhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = rhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));

  auto int_type = context.GetBuiltinType("Int");
  auto float_type = context.GetBuiltinType("Float");

  if (field_type_id == int_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntLess{.type_id = bool_type_id,
                       .lhs_id = lhs_field,
                       .rhs_id = rhs_field}));
  }
  if (field_type_id == float_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FloatLess{.type_id = bool_type_id,
                         .lhs_id = lhs_field,
                         .rhs_id = rhs_field}));
  }
  // Fallback: IntLess
  return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::IntLess{.type_id = bool_type_id,
                     .lhs_id = lhs_field,
                     .rhs_id = rhs_field}));
}

// Helper: emit field greater-than comparison for Comparable.
static auto EmitFieldGreater(Context& context, SemIR::InstId lhs_param,
                             SemIR::InstId rhs_param, int32_t field_idx,
                             SemIR::TypeId field_type_id) -> SemIR::InstId {
  auto bool_type_id = context.GetBuiltinType("Bool");
  auto lhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = lhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));
  auto rhs_field = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FieldAccess{.type_id = field_type_id,
                         .base_id = rhs_param,
                         .index = SemIR::ElementIndex(field_idx)}));

  auto int_type = context.GetBuiltinType("Int");
  auto float_type = context.GetBuiltinType("Float");

  if (field_type_id == int_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntGreater{.type_id = bool_type_id,
                          .lhs_id = lhs_field,
                          .rhs_id = rhs_field}));
  }
  if (field_type_id == float_type) {
    return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FloatGreater{.type_id = bool_type_id,
                            .lhs_id = lhs_field,
                            .rhs_id = rhs_field}));
  }
  // Fallback: IntGreater
  return context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::IntGreater{.type_id = bool_type_id,
                        .lhs_id = lhs_field,
                        .rhs_id = rhs_field}));
}

// ============================================================================
// M109: Equatable Synthesis
// ============================================================================

void SynthesizeEquatable(Context& context, SemIR::InstId type_inst_id,
                         SemIR::NameScopeId scope_id,
                         llvm::ArrayRef<FieldInfo> fields,
                         bool is_enum, int /*enum_case_count*/) {
  // Skip if user already defined == operator.
  if (IsNameInScope(context, scope_id, "__eq__")) return;

  auto self_type_id = TypeIdFor(type_inst_id);
  auto bool_type_id = context.GetBuiltinType("Bool");
  auto type_name = GetTypeNameStr(context, type_inst_id);

  // --- Synthesize == function ---
  auto eq_name_id = MakeName(context,
      (llvm::Twine(type_name) + ".__eq__").str());
  auto eq_scope_name_id = MakeName(context, "__eq__");

  // Create lhs and rhs parameters.
  auto self_ident = context.identifiers().Add("lhs");
  auto lhs_name = SemIR::NameId::ForIdentifier(self_ident);
  auto rhs_ident = context.identifiers().Add("rhs");
  auto rhs_name = SemIR::NameId::ForIdentifier(rhs_ident);

  auto lhs_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = lhs_name}));
  auto rhs_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(1),
                        .pretty_name_id = rhs_name}));

  // Create Function descriptor.
  SemIR::Function eq_fn;
  eq_fn.name_id = eq_name_id;
  eq_fn.parent_scope_id = scope_id;
  eq_fn.return_type_inst_id = context.types().GetTypeInstId(bool_type_id);

  auto params_block = context.inst_blocks().AddPlaceholder();
  llvm::SmallVector<SemIR::InstId> params = {lhs_param, rhs_param};
  context.inst_blocks().ReplacePlaceholder(
      params_block, llvm::ArrayRef<SemIR::InstId>(params));
  eq_fn.call_params_id = params_block;

  auto decl_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      decl_block, llvm::ArrayRef<SemIR::InstId>());
  eq_fn.decl_block_id = decl_block;

  auto eq_function_id = context.functions().Add(eq_fn);

  auto eq_fn_decl = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = eq_function_id,
                          .decl_block_id = decl_block}));

  // Register == in scope.
  context.AddNameToScope(eq_scope_name_id, eq_fn_decl);

  // Emit function body.
  context.SetCurrentFunction(eq_function_id);
  auto fn_scope = context.name_scopes().Add(
      eq_fn_decl, eq_name_id, scope_id);
  context.PushScope(fn_scope);
  context.AddNameToScope(lhs_name, lhs_param);
  context.AddNameToScope(rhs_name, rhs_param);
  auto body_block = context.PushInstBlock();
  context.functions().Get(eq_function_id).body_block_ids.push_back(body_block);

  if (fields.empty() && !is_enum) {
    // No fields: always equal.
    auto true_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::BoolLiteral{.type_id = bool_type_id,
                           .value = SemIR::BoolValue::From(true)}));
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ReturnExpr{.expr_id = true_val,
                          .dest_id = SemIR::InstId::None}));
  } else {
    // Compare all fields with BoolAnd chain.
    SemIR::InstId result = SemIR::InstId::None;
    for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); ++i) {
      auto cmp = EmitFieldEq(context, lhs_param, rhs_param, i,
                             fields[i].type_id);
      if (!result.has_value()) {
        result = cmp;
      } else {
        result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
            SemIR::LocId::None,
            SemIR::BoolAnd{.type_id = bool_type_id,
                           .lhs_id = result,
                           .rhs_id = cmp}));
      }
    }
    if (!result.has_value()) {
      result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::BoolLiteral{.type_id = bool_type_id,
                             .value = SemIR::BoolValue::From(true)}));
    }
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ReturnExpr{.expr_id = result,
                          .dest_id = SemIR::InstId::None}));
  }

  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();

  // --- Synthesize != function ---
  if (!IsNameInScope(context, scope_id, "__neq__")) {
    auto neq_name_id = MakeName(context,
        (llvm::Twine(type_name) + ".__neq__").str());
    auto neq_scope_name_id = MakeName(context, "__neq__");

    auto neq_lhs = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ValueParam{.type_id = self_type_id,
                          .index = SemIR::CallParamIndex(0),
                          .pretty_name_id = lhs_name}));
    auto neq_rhs = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ValueParam{.type_id = self_type_id,
                          .index = SemIR::CallParamIndex(1),
                          .pretty_name_id = rhs_name}));

    SemIR::Function neq_fn;
    neq_fn.name_id = neq_name_id;
    neq_fn.parent_scope_id = scope_id;
    neq_fn.return_type_inst_id = context.types().GetTypeInstId(bool_type_id);

    auto neq_params_block = context.inst_blocks().AddPlaceholder();
    llvm::SmallVector<SemIR::InstId> neq_params = {neq_lhs, neq_rhs};
    context.inst_blocks().ReplacePlaceholder(
        neq_params_block, llvm::ArrayRef<SemIR::InstId>(neq_params));
    neq_fn.call_params_id = neq_params_block;

    auto neq_decl_block = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        neq_decl_block, llvm::ArrayRef<SemIR::InstId>());
    neq_fn.decl_block_id = neq_decl_block;

    auto neq_function_id = context.functions().Add(neq_fn);

    auto neq_fn_decl = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                            .function_id = neq_function_id,
                            .decl_block_id = neq_decl_block}));

    context.AddNameToScope(neq_scope_name_id, neq_fn_decl);

    // Body: return !eq(lhs, rhs)
    context.SetCurrentFunction(neq_function_id);
    auto neq_fn_scope = context.name_scopes().Add(
        neq_fn_decl, neq_name_id, scope_id);
    context.PushScope(neq_fn_scope);
    context.AddNameToScope(lhs_name, neq_lhs);
    context.AddNameToScope(rhs_name, neq_rhs);
    auto neq_body = context.PushInstBlock();
    context.functions().Get(neq_function_id).body_block_ids.push_back(neq_body);

    // Call == then negate.
    auto eq_call = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::Call{.type_id = bool_type_id,
                    .callee_id = eq_fn_decl,
                    .args_id = neq_params_block}));
    auto negated = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::BoolNot{.type_id = bool_type_id,
                       .operand_id = eq_call}));
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ReturnExpr{.expr_id = negated,
                          .dest_id = SemIR::InstId::None}));

    context.PopInstBlock();
    context.ClearCurrentFunction();
    context.PopScope();
  }
}

// ============================================================================
// M109: Comparable Synthesis
// ============================================================================

void SynthesizeComparable(Context& context, SemIR::InstId type_inst_id,
                          SemIR::NameScopeId scope_id,
                          llvm::ArrayRef<FieldInfo> fields) {
  if (IsNameInScope(context, scope_id, "__lt__")) return;

  auto self_type_id = TypeIdFor(type_inst_id);
  auto bool_type_id = context.GetBuiltinType("Bool");
  auto type_name = GetTypeNameStr(context, type_inst_id);

  auto lt_name_id = MakeName(context,
      (llvm::Twine(type_name) + ".__lt__").str());
  auto lt_scope_name_id = MakeName(context, "__lt__");

  auto lhs_ident = context.identifiers().Add("lhs");
  auto lhs_name = SemIR::NameId::ForIdentifier(lhs_ident);
  auto rhs_ident = context.identifiers().Add("rhs");
  auto rhs_name = SemIR::NameId::ForIdentifier(rhs_ident);

  auto lhs_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = lhs_name}));
  auto rhs_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(1),
                        .pretty_name_id = rhs_name}));

  SemIR::Function lt_fn;
  lt_fn.name_id = lt_name_id;
  lt_fn.parent_scope_id = scope_id;
  lt_fn.return_type_inst_id = context.types().GetTypeInstId(bool_type_id);

  auto params_block = context.inst_blocks().AddPlaceholder();
  llvm::SmallVector<SemIR::InstId> params = {lhs_param, rhs_param};
  context.inst_blocks().ReplacePlaceholder(
      params_block, llvm::ArrayRef<SemIR::InstId>(params));
  lt_fn.call_params_id = params_block;

  auto decl_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      decl_block, llvm::ArrayRef<SemIR::InstId>());
  lt_fn.decl_block_id = decl_block;

  auto lt_function_id = context.functions().Add(lt_fn);

  auto lt_fn_decl = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = lt_function_id,
                          .decl_block_id = decl_block}));

  context.AddNameToScope(lt_scope_name_id, lt_fn_decl);

  // Body: lexicographic comparison.
  context.SetCurrentFunction(lt_function_id);
  auto fn_scope = context.name_scopes().Add(
      lt_fn_decl, lt_name_id, scope_id);
  context.PushScope(fn_scope);
  context.AddNameToScope(lhs_name, lhs_param);
  context.AddNameToScope(rhs_name, rhs_param);
  auto body_block = context.PushInstBlock();
  context.functions().Get(lt_function_id).body_block_ids.push_back(body_block);

  if (fields.empty()) {
    // No fields: never less than.
    auto false_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::BoolLiteral{.type_id = bool_type_id,
                           .value = SemIR::BoolValue::From(false)}));
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ReturnExpr{.expr_id = false_val,
                          .dest_id = SemIR::InstId::None}));
  } else {
    // Lexicographic: build from last field backwards.
    // result = false (base case: all equal -> not less)
    auto false_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::BoolLiteral{.type_id = bool_type_id,
                           .value = SemIR::BoolValue::From(false)}));
    SemIR::InstId result = false_val;

    for (int32_t i = static_cast<int32_t>(fields.size()) - 1; i >= 0; --i) {
      auto less = EmitFieldLess(context, lhs_param, rhs_param, i,
                                fields[i].type_id);
      auto greater = EmitFieldGreater(context, lhs_param, rhs_param, i,
                                      fields[i].type_id);
      auto not_greater = context.AddInst(
          SemIR::LocIdAndInst::UncheckedLoc(
              SemIR::LocId::None,
              SemIR::BoolNot{.type_id = bool_type_id,
                             .operand_id = greater}));
      // equal_at_i = !less && !greater
      auto not_less = context.AddInst(
          SemIR::LocIdAndInst::UncheckedLoc(
              SemIR::LocId::None,
              SemIR::BoolNot{.type_id = bool_type_id,
                             .operand_id = less}));
      auto equal_at_i = context.AddInst(
          SemIR::LocIdAndInst::UncheckedLoc(
              SemIR::LocId::None,
              SemIR::BoolAnd{.type_id = bool_type_id,
                             .lhs_id = not_less,
                             .rhs_id = not_greater}));
      // result = less || (equal_at_i && prev_result)
      auto tail = context.AddInst(
          SemIR::LocIdAndInst::UncheckedLoc(
              SemIR::LocId::None,
              SemIR::BoolAnd{.type_id = bool_type_id,
                             .lhs_id = equal_at_i,
                             .rhs_id = result}));
      result = context.AddInst(
          SemIR::LocIdAndInst::UncheckedLoc(
              SemIR::LocId::None,
              SemIR::BoolOr{.type_id = bool_type_id,
                            .lhs_id = less,
                            .rhs_id = tail}));
    }
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::ReturnExpr{.expr_id = result,
                          .dest_id = SemIR::InstId::None}));
  }

  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();
}

// ============================================================================
// M110: Hashable Synthesis
// ============================================================================

void SynthesizeHashable(Context& context, SemIR::InstId type_inst_id,
                        SemIR::NameScopeId scope_id,
                        llvm::ArrayRef<FieldInfo> fields,
                        bool /*is_enum*/, int /*enum_case_count*/) {
  if (IsNameInScope(context, scope_id, "hash")) return;

  auto self_type_id = TypeIdFor(type_inst_id);
  auto int_type_id = context.GetBuiltinType("Int");
  auto type_name = GetTypeNameStr(context, type_inst_id);

  auto hash_mangled_id = MakeName(context,
      (llvm::Twine(type_name) + ".hash").str());
  auto hash_scope_name_id = MakeName(context, "hash");

  auto self_ident = context.identifiers().Add("self");
  auto self_name = SemIR::NameId::ForIdentifier(self_ident);

  auto self_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = self_name}));

  SemIR::Function hash_fn;
  hash_fn.name_id = hash_mangled_id;
  hash_fn.parent_scope_id = scope_id;
  hash_fn.return_type_inst_id = context.types().GetTypeInstId(int_type_id);

  auto params_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      params_block, llvm::ArrayRef<SemIR::InstId>{self_param});
  hash_fn.call_params_id = params_block;

  auto decl_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      decl_block, llvm::ArrayRef<SemIR::InstId>());
  hash_fn.decl_block_id = decl_block;

  auto hash_function_id = context.functions().Add(hash_fn);

  auto hash_fn_decl = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = hash_function_id,
                          .decl_block_id = decl_block}));

  // Register as computed property via ComputedPropertyDecl.
  auto cpd_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ComputedPropertyDecl{.type_id = int_type_id,
                                  .getter_id = hash_fn_decl}));
  context.AddNameToScope(hash_scope_name_id, cpd_id);

  // Body: djb2 hash combining.
  context.SetCurrentFunction(hash_function_id);
  auto fn_scope = context.name_scopes().Add(
      hash_fn_decl, hash_mangled_id, scope_id);
  context.PushScope(fn_scope);
  context.AddNameToScope(self_name, self_param);
  auto body_block = context.PushInstBlock();
  context.functions().Get(hash_function_id).body_block_ids.push_back(
      body_block);

  // hash = 5381 (djb2 seed)
  auto seed_int_id = context.ints().Add(int64_t(5381));
  auto hash_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::IntValue{.type_id = int_type_id, .int_id = seed_int_id}));

  auto thirty_one_int_id = context.ints().Add(int64_t(31));
  auto thirty_one = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::IntValue{.type_id = int_type_id, .int_id = thirty_one_int_id}));

  for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); ++i) {
    // hash = hash * 31 + field_hash
    auto mul = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntMul{.type_id = int_type_id,
                      .lhs_id = hash_val,
                      .rhs_id = thirty_one}));

    // Get field value.
    auto field_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FieldAccess{.type_id = fields[i].type_id,
                           .base_id = self_param,
                           .index = SemIR::ElementIndex(i)}));

    // For Int fields, use the value directly as hash.
    // For String/Bool/Float, convert to Int first.
    SemIR::InstId field_hash = field_val;

    auto string_type = context.GetBuiltinType("String");
    auto bool_type = context.GetBuiltinType("Bool");
    auto float_type = context.GetBuiltinType("Float");

    if (fields[i].type_id == string_type) {
      // String: use StringLen as simple hash (good enough for basic synthesis).
      field_hash = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::StringLen{.type_id = int_type_id,
                           .operand_id = field_val}));
    } else if (fields[i].type_id == bool_type) {
      // Bool -> Int: use the bool as-is since SemIR IntAdd handles mixed sizes.
    } else if (fields[i].type_id == float_type) {
      // Float -> Int conversion for hashing.
      field_hash = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::FloatToInt{.type_id = int_type_id,
                            .operand_id = field_val}));
    }

    hash_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::IntAdd{.type_id = int_type_id,
                      .lhs_id = mul,
                      .rhs_id = field_hash}));
  }

  context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ReturnExpr{.expr_id = hash_val,
                        .dest_id = SemIR::InstId::None}));

  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();
}

// ============================================================================
// M111: CustomStringConvertible Synthesis
// ============================================================================

void SynthesizeDescription(Context& context, SemIR::InstId type_inst_id,
                           SemIR::NameScopeId scope_id,
                           llvm::ArrayRef<FieldInfo> fields,
                           llvm::StringRef type_name) {
  if (IsNameInScope(context, scope_id, "description")) return;

  auto self_type_id = TypeIdFor(type_inst_id);
  auto string_type_id = context.GetBuiltinType("String");

  auto desc_mangled_id = MakeName(context,
      (llvm::Twine(type_name) + ".description").str());
  auto desc_scope_name_id = MakeName(context, "description");

  auto self_ident = context.identifiers().Add("self");
  auto self_name = SemIR::NameId::ForIdentifier(self_ident);

  auto self_param = context.AddInstInNoBlock(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = self_name}));

  SemIR::Function desc_fn;
  desc_fn.name_id = desc_mangled_id;
  desc_fn.parent_scope_id = scope_id;
  desc_fn.return_type_inst_id = context.types().GetTypeInstId(string_type_id);

  auto params_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      params_block, llvm::ArrayRef<SemIR::InstId>{self_param});
  desc_fn.call_params_id = params_block;

  auto decl_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      decl_block, llvm::ArrayRef<SemIR::InstId>());
  desc_fn.decl_block_id = decl_block;

  auto desc_function_id = context.functions().Add(desc_fn);

  auto desc_fn_decl = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = desc_function_id,
                          .decl_block_id = decl_block}));

  // Register as computed property.
  auto cpd_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ComputedPropertyDecl{.type_id = string_type_id,
                                  .getter_id = desc_fn_decl}));
  context.AddNameToScope(desc_scope_name_id, cpd_id);

  // Body: build "TypeName(field1: val1, field2: val2)" string.
  context.SetCurrentFunction(desc_function_id);
  auto fn_scope = context.name_scopes().Add(
      desc_fn_decl, desc_mangled_id, scope_id);
  context.PushScope(fn_scope);
  context.AddNameToScope(self_name, self_param);
  auto body_block = context.PushInstBlock();
  context.functions().Get(desc_function_id).body_block_ids.push_back(
      body_block);

  auto int_type = context.GetBuiltinType("Int");

  // Start with "TypeName("
  SynthNameStorage().push_back(std::string(type_name) + "(");
  auto prefix_str = context.string_literal_values().Add(
      SynthNameStorage().back());
  auto result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::StringLiteral{.type_id = string_type_id,
                           .string_id = prefix_str}));

  for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); ++i) {
    // Add field name prefix: "fieldName: "
    auto field_ident = fields[i].name_id.AsIdentifierId();
    std::string field_label;
    if (field_ident.has_value()) {
      field_label = std::string(context.identifiers().Get(field_ident));
    } else {
      field_label = "?";
    }
    if (i > 0) field_label = ", " + field_label;
    field_label += ": ";

    SynthNameStorage().push_back(field_label);
    auto label_str = context.string_literal_values().Add(
        SynthNameStorage().back());
    auto label_lit = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::StringLiteral{.type_id = string_type_id,
                             .string_id = label_str}));
    result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::StringConcat{.type_id = string_type_id,
                            .lhs_id = result,
                            .rhs_id = label_lit}));

    // Get field value and convert to string.
    auto field_val = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::FieldAccess{.type_id = fields[i].type_id,
                           .base_id = self_param,
                           .index = SemIR::ElementIndex(i)}));

    SemIR::InstId val_str = field_val;
    if (fields[i].type_id == int_type) {
      val_str = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::IntToString{.type_id = string_type_id,
                             .operand_id = field_val}));
    } else if (fields[i].type_id == context.GetBuiltinType("Bool")) {
      // Bool -> string via IntToString (0/1 representation).
      val_str = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::IntToString{.type_id = string_type_id,
                             .operand_id = field_val}));
    } else if (fields[i].type_id == context.GetBuiltinType("Float")) {
      // Float -> Int -> String (simplified).
      auto as_int = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::FloatToInt{.type_id = int_type,
                            .operand_id = field_val}));
      val_str = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
          SemIR::LocId::None,
          SemIR::IntToString{.type_id = string_type_id,
                             .operand_id = as_int}));
    }
    // String fields: val_str = field_val (identity)

    result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId::None,
        SemIR::StringConcat{.type_id = string_type_id,
                            .lhs_id = result,
                            .rhs_id = val_str}));
  }

  // Close with ")"
  auto close_str = context.string_literal_values().Add(")");
  auto close_lit = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::StringLiteral{.type_id = string_type_id,
                           .string_id = close_str}));
  result = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::StringConcat{.type_id = string_type_id,
                          .lhs_id = result,
                          .rhs_id = close_lit}));

  context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId::None,
      SemIR::ReturnExpr{.expr_id = result,
                        .dest_id = SemIR::InstId::None}));

  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();
}

}  // namespace TinySwift::Check
