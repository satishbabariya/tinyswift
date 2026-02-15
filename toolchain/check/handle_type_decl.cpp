// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_type_decl.h"

#include "toolchain/check/handle_stmt.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Extracts the name from a type definition start node.
auto ExtractTypeName(Context& context, Parse::NodeId start_node_id)
    -> std::pair<SemIR::NameId, Parse::NodeId> {
  auto children = context.tree_and_subtrees().children(start_node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
        child_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      return {SemIR::NameId::ForIdentifier(ident_id), child};
    }
  }
  return {SemIR::NameId::None, Parse::NodeId::None};
}

// Processes the members of a type definition body.
auto HandleTypeMembers(Context& context,
                       llvm::iterator_range<Parse::TreeAndSubtrees::SiblingIterator> children)
    -> void {
  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    // Skip the start node and the closing brace.
    if (child_kind == Parse::NodeKind::StructDefinitionStart ||
        child_kind == Parse::NodeKind::ClassDefinitionStart ||
        child_kind == Parse::NodeKind::EnumDefinitionStart ||
        child_kind == Parse::NodeKind::ProtocolDefinitionStart ||
        child_kind == Parse::NodeKind::ExtensionDefinitionStart) {
      continue;
    }

    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                       Parse::NodeCategory::Decl)) {
      HandleStatement(context, child);
    }
  }
}

}  // namespace

auto HandleStructDefinition(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  // Find the start node.
  Parse::NodeId start_node_id = Parse::NodeId::None;
  for (auto child : children) {
    if (context.node_kind(child) == Parse::NodeKind::StructDefinitionStart) {
      start_node_id = child;
      break;
    }
  }

  auto [name_id, name_node_id] =
      start_node_id.has_value()
          ? ExtractTypeName(context, start_node_id)
          : std::pair(SemIR::NameId::None, Parse::NodeId::None);

  // Create a name scope for the struct.
  auto type_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, name_id, context.CurrentScopeId());

  // Emit StructType instruction.
  auto struct_type_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::StructType{.type_id = SemIR::TypeType::TypeId,
                        .name_scope_id = type_scope_id}));

  // Register the type name in the current scope.
  if (name_id.has_value()) {
    context.AddNameToScope(name_id, struct_type_id);
  }

  // Push the type's scope and process members.
  context.PushScope(type_scope_id);
  HandleTypeMembers(context, context.tree_and_subtrees().children(node_id));
  context.PopScope();
}

auto HandleClassDefinition(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  Parse::NodeId start_node_id = Parse::NodeId::None;
  for (auto child : children) {
    if (context.node_kind(child) == Parse::NodeKind::ClassDefinitionStart) {
      start_node_id = child;
      break;
    }
  }

  auto [name_id, name_node_id] =
      start_node_id.has_value()
          ? ExtractTypeName(context, start_node_id)
          : std::pair(SemIR::NameId::None, Parse::NodeId::None);

  auto type_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, name_id, context.CurrentScopeId());

  auto class_type_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::ClassType{.type_id = SemIR::TypeType::TypeId,
                       .name_scope_id = type_scope_id}));

  if (name_id.has_value()) {
    context.AddNameToScope(name_id, class_type_id);
  }

  context.PushScope(type_scope_id);
  HandleTypeMembers(context, context.tree_and_subtrees().children(node_id));
  context.PopScope();
}

auto HandleEnumDefinition(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  Parse::NodeId start_node_id = Parse::NodeId::None;
  for (auto child : children) {
    if (context.node_kind(child) == Parse::NodeKind::EnumDefinitionStart) {
      start_node_id = child;
      break;
    }
  }

  auto [name_id, name_node_id] =
      start_node_id.has_value()
          ? ExtractTypeName(context, start_node_id)
          : std::pair(SemIR::NameId::None, Parse::NodeId::None);

  // For now, register the enum as a namespace.
  auto type_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, name_id, context.CurrentScopeId());

  auto enum_type_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::StructType{.type_id = SemIR::TypeType::TypeId,
                        .name_scope_id = type_scope_id}));

  if (name_id.has_value()) {
    context.AddNameToScope(name_id, enum_type_id);
  }

  context.PushScope(type_scope_id);
  // Process enum cases and members.
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::EnumDefinitionStart) {
      continue;
    }
    if (child_kind == Parse::NodeKind::EnumCaseDecl) {
      // Register each case name in the enum scope.
      auto case_children = context.tree_and_subtrees().children(child);
      for (auto cc : case_children) {
        if (context.node_kind(cc) == Parse::NodeKind::EnumCaseElement) {
          auto elem_children = context.tree_and_subtrees().children(cc);
          for (auto ec : elem_children) {
            if (context.node_kind(ec) ==
                Parse::NodeKind::IdentifierNameNotBeforeParams) {
              auto token = context.node_token(ec);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              auto case_name_id = SemIR::NameId::ForIdentifier(ident_id);
              // Register as a value binding.
              auto entity_name_id = context.entity_names().Add(
                  {.name_id = case_name_id,
                   .parent_scope_id = type_scope_id});
              auto binding_id = context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(ec),
                  SemIR::ValueBinding{
                      .type_id = SemIR::TypeId::ForTypeConstant(
                          SemIR::ConstantId::ForConcreteConstant(
                              enum_type_id)),
                      .entity_name_id = entity_name_id,
                      .value_id = SemIR::InstId::None}));
              context.AddNameToScope(case_name_id, binding_id);
            }
          }
        }
      }
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Decl)) {
      HandleStatement(context, child);
    }
  }
  context.PopScope();
}

auto HandleProtocolDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  Parse::NodeId start_node_id = Parse::NodeId::None;
  for (auto child : children) {
    if (context.node_kind(child) == Parse::NodeKind::ProtocolDefinitionStart) {
      start_node_id = child;
      break;
    }
  }

  auto [name_id, name_node_id] =
      start_node_id.has_value()
          ? ExtractTypeName(context, start_node_id)
          : std::pair(SemIR::NameId::None, Parse::NodeId::None);

  auto type_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, name_id, context.CurrentScopeId());

  // Protocols are stubs for now - just register the name.
  auto proto_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::StructType{.type_id = SemIR::TypeType::TypeId,
                        .name_scope_id = type_scope_id}));

  if (name_id.has_value()) {
    context.AddNameToScope(name_id, proto_id);
  }

  context.PushScope(type_scope_id);
  HandleTypeMembers(context, context.tree_and_subtrees().children(node_id));
  context.PopScope();
}

auto HandleExtensionDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  // Extensions add to an existing type's scope. For now, just process members
  // in the current scope.
  auto children = context.tree_and_subtrees().children(node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ExtensionDefinitionStart) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                       Parse::NodeCategory::Decl)) {
      HandleStatement(context, child);
    }
  }
}

}  // namespace TinySwift::Check
