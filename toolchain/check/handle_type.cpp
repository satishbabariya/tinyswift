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

    // M67: During specialization, check type parameter bindings first.
    if (context.is_specializing()) {
      auto ident_id_lookup = context.identifiers().Lookup(text);
      if (ident_id_lookup.has_value()) {
        auto name_id = SemIR::NameId::ForIdentifier(ident_id_lookup);
        auto it = context.type_param_bindings().find(name_id.index);
        if (it != context.type_param_bindings().end()) {
          return it->second;  // Concrete type for this type parameter.
        }
      }
    }

    // Check typealias map first.
    auto ident_id_opt = context.identifiers().Lookup(text);
    if (ident_id_opt.has_value()) {
      auto name_id = SemIR::NameId::ForIdentifier(ident_id_opt);
      auto alias_it = context.typealias_map().find(name_id.index);
      if (alias_it != context.typealias_map().end()) {
        return alias_it->second;
      }
    }

    // M69: Check if this is a generic type template with pending type args.
    if (ident_id_opt.has_value()) {
      auto name_id = SemIR::NameId::ForIdentifier(ident_id_opt);
      auto tmpl_it = context.generic_type_templates().find(name_id.index);
      if (tmpl_it != context.generic_type_templates().end()) {
        // This is a generic type — specialization will be triggered when
        // concrete type args are available at the call site (HandleCallExpr).
        // For now, return the original type inst as a placeholder TypeId.
        auto orig_id = tmpl_it->second.original_type_inst_id;
        if (orig_id.has_value()) {
          return SemIR::TypeId::ForTypeConstant(
              SemIR::ConstantId::ForConcreteConstant(orig_id));
        }
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
    // Function types are represented as opaque pointers at runtime (in LLVM 20,
    // all pointers are ptr).  Create a PointerType to Int as a stand-in — it
    // lowers to ptr, which is the correct ABI for function pointers.
    auto int_type_inst_id =
        context.types().GetTypeInstId(context.GetBuiltinType("Int"));
    auto inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst::NoLoc(
        SemIR::PointerType{.type_id = SemIR::TypeType::TypeId,
                           .pointee_id = int_type_inst_id}));
    return SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(inst_id));
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
