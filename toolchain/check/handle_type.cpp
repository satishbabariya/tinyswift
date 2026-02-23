// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_type.h"

#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

auto HandleTypeExpr(Context& context, Parse::NodeId node_id) -> SemIR::TypeId {
  auto kind = context.node_kind(node_id);

  if (kind == Parse::NodeKind::IdentifierType) {
    auto token = context.node_token(node_id);
    auto text = context.token_text(token);

    // Check typealias map first.
    auto ident_id_opt = context.identifiers().Lookup(text);
    if (ident_id_opt.has_value()) {
      auto name_id = SemIR::NameId::ForIdentifier(ident_id_opt);
      auto alias_it = context.typealias_map().find(name_id.index);
      if (alias_it != context.typealias_map().end()) {
        return alias_it->second;
      }
    }

    auto result = context.GetBuiltinType(text);
    if (result != SemIR::ErrorInst::TypeId) {
      return result;
    }
    // Fall through to scope lookup for user-defined types.
    auto ident_id = context.identifiers().Lookup(text);
    if (ident_id.has_value()) {
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      auto inst_id = context.LookupName(name_id);
      if (inst_id.has_value()) {
        auto inst = context.insts().Get(inst_id);
        if (inst.Is<SemIR::StructType>() || inst.Is<SemIR::ClassType>() ||
            inst.Is<SemIR::EnumDecl>()) {
          return SemIR::TypeId::ForTypeConstant(
              SemIR::ConstantId::ForConcreteConstant(inst_id));
        }
      }
    }
    return SemIR::ErrorInst::TypeId;
  }

  if (kind == Parse::NodeKind::TypeAnnotation) {
    // TypeAnnotation has one child: the type expression.
    auto children = context.children_source_order(node_id);
    for (auto child : children) {
      if (context.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Type)) {
        return HandleTypeExpr(context, child);
      }
    }
    return SemIR::ErrorInst::TypeId;
  }

  if (kind == Parse::NodeKind::OptionalType) {
    // OptionalType has one child: the base type.
    auto children = context.children_source_order(node_id);
    SemIR::TypeId inner_type_id = SemIR::ErrorInst::TypeId;
    for (auto child : children) {
      if (context.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Type)) {
        inner_type_id = HandleTypeExpr(context, child);
        break;
      }
    }
    // Emit an OptionalType instruction.
    auto inner_type_inst_id =
        context.types().GetTypeInstId(inner_type_id);
    auto inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::OptionalType{.type_id = SemIR::TypeType::TypeId,
                            .inner_type_id = inner_type_inst_id}));
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(inst_id));
  }

  // TupleType node has child_count=0 (element types are siblings in the
  // parent, not children of TupleType). When reached here, we return an
  // empty tuple type; the TupleExpr handler builds the real type from values.
  if (kind == Parse::NodeKind::TupleType) {
    auto block = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        block, llvm::ArrayRef<SemIR::InstId>());
    auto id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::TupleType{.type_id = SemIR::TypeType::TypeId,
                         .element_types_id = block}));
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(id));
  }

  if (kind == Parse::NodeKind::ArrayType) {
    // For now, arrays are just a pointer to the element type.
    auto children = context.children_source_order(node_id);
    for (auto child : children) {
      if (context.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Type)) {
        return HandleTypeExpr(context, child);
      }
    }
    return SemIR::ErrorInst::TypeId;
  }

  if (kind == Parse::NodeKind::FunctionType) {
    // Stub: function types not fully supported yet.
    return SemIR::ErrorInst::TypeId;
  }

  // Fallback: try to treat as an identifier.
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);
  auto result = context.GetBuiltinType(text);
  if (result != SemIR::ErrorInst::TypeId) {
    return result;
  }

  // Check if this is a user-defined type by looking it up in scope.
  auto ident_id = context.identifiers().Lookup(text);
  if (ident_id.has_value()) {
    auto name_id = SemIR::NameId::ForIdentifier(ident_id);
    auto inst_id = context.LookupName(name_id);
    if (inst_id.has_value()) {
      auto inst = context.insts().Get(inst_id);
      if (inst.Is<SemIR::StructType>() || inst.Is<SemIR::ClassType>() ||
          inst.Is<SemIR::EnumDecl>()) {
        return SemIR::TypeId::ForTypeConstant(
            SemIR::ConstantId::ForConcreteConstant(inst_id));
      }
    }
  }

  return SemIR::ErrorInst::TypeId;
}

}  // namespace TinySwift::Check
