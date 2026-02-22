// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_type_decl.h"

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Extracts the name from a type definition start node.
auto ExtractTypeName(Context& context, Parse::NodeId start_node_id)
    -> std::pair<SemIR::NameId, Parse::NodeId> {
  auto children = context.children_source_order(start_node_id);
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

// Collects field information from var declarations inside a struct/class body.
// Returns a list of (name_id, type_id) pairs for field descriptors.
struct FieldInfo {
  SemIR::NameId name_id;
  SemIR::TypeId type_id;
  Parse::NodeId node_id;
};

// Extracts field info from a single VariableDecl node.
static auto ExtractFieldFromVarDecl(Context& context, Parse::NodeId child)
    -> std::optional<FieldInfo> {
  SemIR::NameId field_name_id = SemIR::NameId::None;
  SemIR::TypeId field_type_id = SemIR::TypeId::None;
  Parse::NodeId field_node_id = child;

  auto var_children = context.children_source_order(child);
  for (auto vc : var_children) {
    auto vc_kind = context.node_kind(vc);

    // Struct member style: `var x: Int` has IdentifierPattern directly.
    if (vc_kind == Parse::NodeKind::IdentifierPattern) {
      field_node_id = vc;
      auto token = context.node_token(vc);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      field_name_id = SemIR::NameId::ForIdentifier(ident_id);
      continue;
    }

    // Struct member type annotation: `var x: Int` has TypeAnnotation.
    if (vc_kind == Parse::NodeKind::TypeAnnotation ||
        vc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
      field_type_id = HandleTypeExpr(context, vc);
      continue;
    }

    // Local var style: `var x: Int = ...` wraps in VariablePattern.
    if (vc_kind == Parse::NodeKind::VariablePattern) {
      auto vp_children = context.children_source_order(vc);
      for (auto vpc : vp_children) {
        if (context.node_kind(vpc) == Parse::NodeKind::VarBindingPattern) {
          auto bp_children = context.children_source_order(vpc);
          for (auto bpc : bp_children) {
            auto bpc_kind = context.node_kind(bpc);
            if (bpc_kind ==
                Parse::NodeKind::IdentifierNameNotBeforeParams) {
              field_node_id = bpc;
              auto token = context.node_token(bpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              field_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (bpc_kind.category().HasAnyOf(
                           Parse::NodeCategory::Type) ||
                       bpc_kind == Parse::NodeKind::TypeAnnotation) {
              field_type_id = HandleTypeExpr(context, bpc);
            }
          }
        }
      }
    }
  }

  if (!field_name_id.has_value()) return std::nullopt;
  return FieldInfo{
      .name_id = field_name_id,
      .type_id = field_type_id.has_value() ? field_type_id
                                           : SemIR::ErrorInst::TypeId,
      .node_id = field_node_id};
}

auto CollectFieldDecls(Context& context,
                       llvm::ArrayRef<Parse::NodeId> children)
    -> llvm::SmallVector<FieldInfo> {
  llvm::SmallVector<FieldInfo> fields;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    // Skip start nodes.
    if (child_kind == Parse::NodeKind::StructDefinitionStart ||
        child_kind == Parse::NodeKind::ClassDefinitionStart) {
      continue;
    }

    // Struct members may be wrapped in a CodeBlock — recurse into it.
    if (child_kind == Parse::NodeKind::CodeBlock) {
      auto block_children = context.children_source_order(child);
      for (auto bc : block_children) {
        if (context.node_kind(bc) == Parse::NodeKind::CodeBlockStart) continue;
        if (context.node_kind(bc) == Parse::NodeKind::VariableDecl) {
          if (auto fi = ExtractFieldFromVarDecl(context, bc)) {
            fields.push_back(*fi);
          }
        }
      }
      continue;
    }

    // Look for VariableDecl (stored properties) as direct children.
    if (child_kind == Parse::NodeKind::VariableDecl) {
      if (auto fi = ExtractFieldFromVarDecl(context, child)) {
        fields.push_back(*fi);
      }
    }
  }
  return fields;
}

// Processes the members of a type definition body.
auto HandleTypeMembers(Context& context,
                       llvm::ArrayRef<Parse::NodeId> children) -> void {
  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    // Skip the start node.
    if (child_kind == Parse::NodeKind::StructDefinitionStart ||
        child_kind == Parse::NodeKind::ClassDefinitionStart ||
        child_kind == Parse::NodeKind::EnumDefinitionStart ||
        child_kind == Parse::NodeKind::ProtocolDefinitionStart ||
        child_kind == Parse::NodeKind::ExtensionDefinitionStart) {
      continue;
    }

    // Members may be wrapped in a CodeBlock — recurse into it.
    if (child_kind == Parse::NodeKind::CodeBlock) {
      auto block_children = context.children_source_order(child);
      for (auto bc : block_children) {
        auto bc_kind = context.node_kind(bc);
        if (bc_kind == Parse::NodeKind::CodeBlockStart) continue;
        // Skip VariableDecl — already registered as StructField above.
        if (bc_kind == Parse::NodeKind::VariableDecl) continue;
        if (bc_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                        Parse::NodeCategory::Decl)) {
          HandleStatement(context, bc);
        }
      }
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
  auto children = context.children_source_order(node_id);

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

  // Collect field declarations before processing members.
  auto field_infos = CollectFieldDecls(context, children);

  // Emit StructField instructions and register fields in the struct scope.
  context.PushScope(type_scope_id);
  int32_t field_index = 0;
  for (auto& fi : field_infos) {
    auto field_inst_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(fi.node_id),
        SemIR::StructField{.type_id = fi.type_id,
                           .name_id = fi.name_id,
                           .index = SemIR::ElementIndex(field_index)}));
    context.AddNameToScope(fi.name_id, field_inst_id);
    ++field_index;
  }

  // Now process all members (this will handle var decls again, plus methods).
  HandleTypeMembers(context, children);
  context.PopScope();
}

auto HandleClassDefinition(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

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

  // Collect and register fields.
  auto field_infos = CollectFieldDecls(context, children);

  context.PushScope(type_scope_id);
  int32_t field_index = 0;
  for (auto& fi : field_infos) {
    auto field_inst_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(fi.node_id),
        SemIR::StructField{.type_id = fi.type_id,
                           .name_id = fi.name_id,
                           .index = SemIR::ElementIndex(field_index)}));
    context.AddNameToScope(fi.name_id, field_inst_id);
    ++field_index;
  }

  HandleTypeMembers(context, children);
  context.PopScope();
}

auto HandleEnumDefinition(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

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

  auto type_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, name_id, context.CurrentScopeId());

  // Create cases block placeholder.
  auto cases_block_id = context.inst_blocks().AddPlaceholder();

  // Emit EnumDecl instruction.
  auto enum_type_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::EnumDecl{.type_id = SemIR::TypeType::TypeId,
                      .name_scope_id = type_scope_id,
                      .cases_id = cases_block_id}));

  if (name_id.has_value()) {
    context.AddNameToScope(name_id, enum_type_id);
  }

  // Get the enum's TypeId for case values.
  auto enum_value_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(enum_type_id));

  context.PushScope(type_scope_id);

  // Process enum cases. EnumCaseDecl nodes live inside a CodeBlock child
  // of EnumDefinition, NOT as direct children (same pattern as struct fields).
  // Helper lambda to process a list of potential enum case nodes.
  llvm::SmallVector<SemIR::InstId> case_inst_ids;
  int32_t discriminant = 0;

  auto ProcessEnumCaseNodes = [&](auto& node_children) {
    for (auto child : node_children) {
      auto child_kind = context.node_kind(child);
      if (child_kind == Parse::NodeKind::EnumCaseDecl) {
        auto case_children = context.children_source_order(child);
        for (auto cc : case_children) {
          if (context.node_kind(cc) == Parse::NodeKind::EnumCaseElement) {
            auto elem_children = context.children_source_order(cc);
            for (auto ec : elem_children) {
              if (context.node_kind(ec) ==
                  Parse::NodeKind::IdentifierNameNotBeforeParams) {
                auto token = context.node_token(ec);
                auto text = context.token_text(token);
                auto ident_id = context.identifiers().Add(text);
                auto case_name_id = SemIR::NameId::ForIdentifier(ident_id);

                // Emit EnumCase instruction.
                auto case_id = context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(ec),
                    SemIR::EnumCase{
                        .type_id = enum_value_type_id,
                        .name_id = case_name_id,
                        .discriminant = SemIR::ElementIndex(discriminant)}));
                case_inst_ids.push_back(case_id);
                context.AddNameToScope(case_name_id, case_id);
                ++discriminant;
              }
            }
          }
        }
      }
    }
  };

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::EnumDefinitionStart) {
      continue;
    }
    if (child_kind == Parse::NodeKind::CodeBlock) {
      // EnumCaseDecl nodes are inside the CodeBlock, not direct children.
      auto block_children = context.children_source_order(child);
      ProcessEnumCaseNodes(block_children);
    } else if (child_kind == Parse::NodeKind::EnumCaseDecl) {
      // Direct EnumCaseDecl (if parser ever emits them without CodeBlock).
      auto single = llvm::SmallVector<Parse::NodeId>{child};
      ProcessEnumCaseNodes(single);
    }
  }

  // Finalize cases block.
  context.inst_blocks().ReplacePlaceholder(
      cases_block_id, llvm::ArrayRef<SemIR::InstId>(case_inst_ids));

  context.PopScope();
}

auto HandleProtocolDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  auto children = context.children_source_order(node_id);

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
  HandleTypeMembers(context, children);
  context.PopScope();
}

auto HandleExtensionDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  // Extensions add to an existing type's scope. For now, just process members
  // in the current scope.
  auto children = context.children_source_order(node_id);
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
