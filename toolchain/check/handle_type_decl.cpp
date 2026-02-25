// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_type_decl.h"

#include <deque>

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_generic.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// M69: Extracts generic parameter info from a type definition start node
// (StructDefinitionStart, ClassDefinitionStart, EnumDefinitionStart).
auto ExtractTypeGenericParams(Context& context, Parse::NodeId start_node_id)
    -> llvm::SmallVector<SemIR::Function::GenericParamInfo> {
  llvm::SmallVector<SemIR::Function::GenericParamInfo> result;
  auto children = context.children_source_order(start_node_id);

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::GenericParameterClause) {
      auto clause_children = context.children_source_order(child);
      SemIR::Function::GenericParamInfo pending_info;
      for (auto cc : clause_children) {
        auto cc_kind = context.node_kind(cc);
        if (cc_kind == Parse::NodeKind::GenericParameterClauseStart ||
            cc_kind == Parse::NodeKind::PatternListComma) {
          continue;
        }
        if (cc_kind == Parse::NodeKind::GenericParameter) {
          if (pending_info.name_id.has_value()) {
            result.push_back(pending_info);
          }
          auto token = context.node_token(cc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_info = SemIR::Function::GenericParamInfo{};
          pending_info.name_id = SemIR::NameId::ForIdentifier(ident_id);
        } else if (cc_kind == Parse::NodeKind::IdentifierType) {
          auto token = context.node_token(cc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_info.constraint_name_id = SemIR::NameId::ForIdentifier(ident_id);
        }
      }
      if (pending_info.name_id.has_value()) {
        result.push_back(pending_info);
      }
      break;
    }
  }
  return result;
}

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

// Returns true if a VariableDecl node represents a computed property
// (i.e., it has an AccessorBlock among its direct children).
static auto IsComputedProperty(Context& context, Parse::NodeId var_decl_node)
    -> bool {
  auto var_children = context.children_source_order(var_decl_node);
  for (auto vc : var_children) {
    if (context.node_kind(vc) == Parse::NodeKind::AccessorBlock) {
      return true;
    }
  }
  return false;
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
          // Skip computed properties — they are not stored fields.
          if (IsComputedProperty(context, bc)) continue;
          if (auto fi = ExtractFieldFromVarDecl(context, bc)) {
            fields.push_back(*fi);
          }
        }
      }
      continue;
    }

    // Look for VariableDecl (stored properties) as direct children.
    if (child_kind == Parse::NodeKind::VariableDecl) {
      if (IsComputedProperty(context, child)) continue;
      if (auto fi = ExtractFieldFromVarDecl(context, child)) {
        fields.push_back(*fi);
      }
    }
  }
  return fields;
}

// Generates synthetic getter (and optionally setter) functions for a computed
// property. The getter has signature:
//   func TypeName.propName(self: TypeName) -> PropType
// The setter (M57) has signature:
//   func TypeName.propName.set(self: inout TypeName, newValue: PropType) -> Void
// and is registered in the type scope under "propName.set".
static auto GenerateComputedPropertyGetter(
    Context& context,
    Parse::NodeId name_node, Parse::NodeId type_node,
    Parse::NodeId accessor_block) -> void {
  // Extract property name from IdentifierPattern.
  auto name_token = context.node_token(name_node);
  auto name_text = context.token_text(name_token);
  auto name_ident_id = context.identifiers().Add(name_text);
  auto prop_name_id = SemIR::NameId::ForIdentifier(name_ident_id);

  // Extract return type.
  auto prop_type_id = (type_node.has_value())
      ? HandleTypeExpr(context, type_node)
      : SemIR::ErrorInst::TypeId;
  if (!prop_type_id.has_value()) {
    prop_type_id = SemIR::ErrorInst::TypeId;
  }

  // Determine enclosing type name for mangling.
  auto enclosing_type_id = context.CurrentTypeInstId();
  llvm::StringRef type_name;
  if (enclosing_type_id.has_value()) {
    auto type_inst = context.insts().Get(enclosing_type_id);
    if (auto st = type_inst.TryAs<SemIR::StructType>()) {
      auto& scope = context.name_scopes().Get(st->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) type_name = context.identifiers().Get(ident_opt);
    } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
      auto& scope = context.name_scopes().Get(ct->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) type_name = context.identifiers().Get(ident_opt);
    }
  }

  // Use std::deque so push_back never invalidates existing string addresses.
  // (SmallVector may reallocate, which would invalidate StringRefs stored in
  // the identifier table.)
  static std::deque<std::string>* prop_name_storage =
      new std::deque<std::string>();

  // Build mangled getter name "TypeName.propName".
  SemIR::NameId getter_mangled_id = prop_name_id;
  if (!type_name.empty()) {
    prop_name_storage->push_back(
        std::string(type_name) + "." + std::string(name_text));
    getter_mangled_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(prop_name_storage->back()));
  }

  // Detect explicit get/set accessors in the accessor_block.
  // In the parse tree, CodeBlock comes BEFORE its accessor keyword:
  //   [AccessorBlockStart] [CodeBlock(getter)] [GetAccessor] [CodeBlock(setter)] [SetAccessor]
  // So we collect CodeBlocks in source order and assign them to the next keyword.
  Parse::NodeId getter_body = Parse::NodeId::None;
  Parse::NodeId setter_body = Parse::NodeId::None;
  Parse::NodeId pending_code_block = Parse::NodeId::None;
  bool has_explicit_accessors = false;

  for (auto ac : context.children_source_order(accessor_block)) {
    auto ac_kind = context.node_kind(ac);
    if (ac_kind == Parse::NodeKind::AccessorBlockStart ||
        ac_kind == Parse::NodeKind::CodeBlockStart) {
      continue;
    }
    if (ac_kind == Parse::NodeKind::CodeBlock) {
      pending_code_block = ac;
    } else if (ac_kind == Parse::NodeKind::GetAccessor) {
      has_explicit_accessors = true;
      if (pending_code_block.has_value()) {
        getter_body = pending_code_block;
        pending_code_block = Parse::NodeId::None;
      }
    } else if (ac_kind == Parse::NodeKind::SetAccessor) {
      has_explicit_accessors = true;
      if (pending_code_block.has_value()) {
        setter_body = pending_code_block;
        pending_code_block = Parse::NodeId::None;
      }
    }
  }
  // Fallback: no explicit accessors — the whole accessor_block is the getter body.
  if (!has_explicit_accessors) {
    getter_body = accessor_block;
  }

  // Synthesize `self` ValueParam for getter (read-only).
  auto self_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(enclosing_type_id));
  auto self_ident_id = context.identifiers().Add("self");
  auto self_name_id = SemIR::NameId::ForIdentifier(self_ident_id);

  auto getter_self_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
      SemIR::LocId(name_node),
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = self_name_id}));

  // Create the getter Function descriptor.
  SemIR::Function getter_fn;
  getter_fn.name_id = getter_mangled_id;
  getter_fn.parent_scope_id = context.CurrentScopeId();
  getter_fn.return_type_inst_id = context.types().GetTypeInstId(prop_type_id);

  auto getter_params_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      getter_params_block_id,
      llvm::ArrayRef<SemIR::InstId>{getter_self_param_id});
  getter_fn.call_params_id = getter_params_block_id;

  auto getter_decl_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(getter_decl_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());
  getter_fn.decl_block_id = getter_decl_block_id;

  auto getter_function_id = context.functions().Add(getter_fn);

  auto getter_fn_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId(name_node),
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = getter_function_id,
                          .decl_block_id = getter_decl_block_id}));

  // Emit ComputedPropertyDecl and register it under the property name.
  auto cpd_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node),
      SemIR::ComputedPropertyDecl{.type_id = prop_type_id,
                                  .getter_id = getter_fn_decl_id}));
  context.AddNameToScope(prop_name_id, cpd_id);

  // Set up the getter function body.
  context.SetCurrentFunction(getter_function_id);
  auto getter_scope_id = context.name_scopes().Add(
      getter_fn_decl_id, getter_mangled_id, context.CurrentScopeId());
  context.PushScope(getter_scope_id);
  context.AddNameToScope(self_name_id, getter_self_param_id);
  auto getter_block_id = context.PushInstBlock();
  context.functions().Get(getter_function_id).body_block_ids.push_back(
      getter_block_id);
  HandleCodeBlock(context, getter_body);
  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();

  // M57: Generate setter if SetAccessor was found.
  if (setter_body.has_value()) {
    // Build mangled setter name "TypeName.propName.set".
    prop_name_storage->push_back(
        std::string(type_name) + "." + std::string(name_text) + ".set");
    auto setter_mangled_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(prop_name_storage->back()));

    // Register the setter name in the type scope for lookup by HandleAssignmentExpr.
    prop_name_storage->push_back(std::string(name_text) + ".set");
    auto setter_lookup_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(prop_name_storage->back()));

    // Synthesize `self` InoutParam for setter (needs to write through).
    auto setter_self_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(name_node),
        SemIR::InoutParam{.type_id = self_type_id,
                          .index = SemIR::CallParamIndex(0),
                          .name_id = self_name_id}));

    // Synthesize `newValue` ValueParam.
    auto new_value_ident_id = context.identifiers().Add("newValue");
    auto new_value_name_id = SemIR::NameId::ForIdentifier(new_value_ident_id);
    auto setter_newvalue_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(name_node),
        SemIR::ValueParam{.type_id = prop_type_id,
                          .index = SemIR::CallParamIndex(1),
                          .pretty_name_id = new_value_name_id}));

    // Create setter Function descriptor.
    SemIR::Function setter_fn;
    setter_fn.name_id = setter_mangled_id;
    setter_fn.parent_scope_id = context.CurrentScopeId();
    setter_fn.is_mutating = true;
    // Setter returns void (no return_type_inst_id).

    auto setter_params_block_id = context.inst_blocks().AddPlaceholder();
    llvm::SmallVector<SemIR::InstId> setter_params = {
        setter_self_param_id, setter_newvalue_param_id};
    context.inst_blocks().ReplacePlaceholder(
        setter_params_block_id,
        llvm::ArrayRef<SemIR::InstId>(setter_params));
    setter_fn.call_params_id = setter_params_block_id;

    auto setter_decl_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(setter_decl_block_id,
                                             llvm::ArrayRef<SemIR::InstId>());
    setter_fn.decl_block_id = setter_decl_block_id;

    auto setter_function_id = context.functions().Add(setter_fn);

    auto setter_fn_decl_id = context.AddInst(
        SemIR::LocIdAndInst::UncheckedLoc(
            SemIR::LocId(name_node),
            SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                                .function_id = setter_function_id,
                                .decl_block_id = setter_decl_block_id}));

    // Register setter under "propName.set" in the type scope.
    context.AddNameToScope(setter_lookup_id, setter_fn_decl_id);

    // Set up the setter function body.
    context.SetCurrentFunction(setter_function_id);
    auto setter_scope_id = context.name_scopes().Add(
        setter_fn_decl_id, setter_mangled_id, context.CurrentScopeId());
    context.PushScope(setter_scope_id);
    context.AddNameToScope(self_name_id, setter_self_param_id);
    context.AddNameToScope(new_value_name_id, setter_newvalue_param_id);
    auto setter_block_id = context.PushInstBlock();
    context.functions().Get(setter_function_id).body_block_ids.push_back(
        setter_block_id);
    HandleCodeBlock(context, setter_body);
    context.PopInstBlock();
    context.ClearCurrentFunction();
    context.PopScope();
  }
}

// M64: Generates getter (and optionally setter) functions for a subscript
// declaration: `subscript(i: Int) -> T { get { ... } set { ... } }`.
// Getter: TypeName.subscript.get(self: T, i: I) -> V
// Setter: TypeName.subscript.set(self: inout T, i: I, newValue: V)
static auto HandleSubscriptDefinition(Context& context,
                                      Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // Find SubscriptDefinitionStart and AccessorBlock.
  Parse::NodeId start_node_id = Parse::NodeId::None;
  Parse::NodeId accessor_block = Parse::NodeId::None;
  for (auto child : children) {
    auto ck = context.node_kind(child);
    if (ck == Parse::NodeKind::SubscriptDefinitionStart) start_node_id = child;
    if (ck == Parse::NodeKind::AccessorBlock) accessor_block = child;
  }
  if (!start_node_id.has_value()) return;

  // Extract return type and params from SubscriptDefinitionStart.
  SemIR::TypeId return_type_id = SemIR::ErrorInst::TypeId;
  llvm::SmallVector<std::pair<SemIR::NameId, SemIR::TypeId>> params;

  for (auto sc : context.children_source_order(start_node_id)) {
    auto ck = context.node_kind(sc);
    if (ck == Parse::NodeKind::ReturnType ||
        ck.category().HasAnyOf(Parse::NodeCategory::Type)) {
      // Try to get the type from ReturnType's children.
      if (ck == Parse::NodeKind::ReturnType) {
        for (auto rc : context.children_source_order(sc)) {
          if (context.node_kind(rc).category().HasAnyOf(
                  Parse::NodeCategory::Type) ||
              context.node_kind(rc) == Parse::NodeKind::IdentifierType) {
            return_type_id = HandleTypeExpr(context, rc);
            break;
          }
        }
      } else {
        return_type_id = HandleTypeExpr(context, sc);
      }
    }
    if (ck == Parse::NodeKind::ExplicitParamList) {
      for (auto pc : context.children_source_order(sc)) {
        if (context.node_kind(pc) == Parse::NodeKind::FunctionParam) {
          SemIR::NameId pname = SemIR::NameId::None;
          SemIR::TypeId ptype = SemIR::ErrorInst::TypeId;
          for (auto fpc : context.children_source_order(pc)) {
            auto fck = context.node_kind(fpc);
            if (fck == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                fck == Parse::NodeKind::IdentifierNameBeforeParams ||
                fck == Parse::NodeKind::IdentifierPattern) {
              auto txt = context.token_text(context.node_token(fpc));
              if (!txt.empty() && txt != "_") {
                pname = SemIR::NameId::ForIdentifier(context.identifiers().Add(txt));
              }
            } else if (fck == Parse::NodeKind::TypeAnnotation ||
                       fck.category().HasAnyOf(Parse::NodeCategory::Type)) {
              ptype = HandleTypeExpr(context, fpc);
            }
          }
          if (pname.has_value()) params.push_back({pname, ptype});
        }
      }
    }
  }

  // Get enclosing type name.
  auto enclosing_type_id = context.CurrentTypeInstId();
  if (!enclosing_type_id.has_value()) return;
  auto type_inst = context.insts().Get(enclosing_type_id);
  llvm::StringRef type_name;
  if (auto st = type_inst.TryAs<SemIR::StructType>()) {
    auto& scope = context.name_scopes().Get(st->name_scope_id);
    auto id_opt = scope.name_id.AsIdentifierId();
    if (id_opt.has_value()) type_name = context.identifiers().Get(id_opt);
  } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
    auto& scope = context.name_scopes().Get(ct->name_scope_id);
    auto id_opt = scope.name_id.AsIdentifierId();
    if (id_opt.has_value()) type_name = context.identifiers().Get(id_opt);
  }

  static std::deque<std::string>* sub_name_storage = new std::deque<std::string>();

  // Build getter function name "TypeName.subscript.get".
  sub_name_storage->push_back(std::string(type_name) + ".subscript.get");
  auto getter_mangled_id = SemIR::NameId::ForIdentifier(
      context.identifiers().Add(sub_name_storage->back()));

  auto self_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(enclosing_type_id));
  auto self_ident_id = context.identifiers().Add("self");
  auto self_name_id = SemIR::NameId::ForIdentifier(self_ident_id);

  // Build getter params: [self: T, i: I, ...].
  llvm::SmallVector<SemIR::InstId> getter_param_ids;
  auto getter_self_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ValueParam{.type_id = self_type_id,
                        .index = SemIR::CallParamIndex(0),
                        .pretty_name_id = self_name_id}));
  getter_param_ids.push_back(getter_self_id);
  for (size_t i = 0; i < params.size(); ++i) {
    auto pi = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ValueParam{.type_id = params[i].second,
                          .index = SemIR::CallParamIndex(static_cast<int32_t>(i + 1)),
                          .pretty_name_id = params[i].first}));
    getter_param_ids.push_back(pi);
  }

  // Create getter SemIR Function.
  SemIR::Function getter_fn;
  getter_fn.name_id = getter_mangled_id;
  getter_fn.parent_scope_id = context.CurrentScopeId();
  getter_fn.return_type_inst_id = context.types().GetTypeInstId(return_type_id);
  auto g_params_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      g_params_id, llvm::ArrayRef<SemIR::InstId>(getter_param_ids));
  getter_fn.call_params_id = g_params_id;
  auto g_decl_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(g_decl_id, llvm::ArrayRef<SemIR::InstId>());
  getter_fn.decl_block_id = g_decl_id;
  auto getter_fn_id = context.functions().Add(getter_fn);

  auto getter_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId(node_id),
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = getter_fn_id,
                          .decl_block_id = g_decl_id}));

  // Register getter under "subscript" in type scope.
  auto sub_ident_id = context.identifiers().Add("subscript");
  auto sub_name_id = SemIR::NameId::ForIdentifier(sub_ident_id);
  context.AddNameToScope(sub_name_id, getter_decl_id);

  // Also register the return type for SubscriptExpr callers.
  // (HandleSubscriptExpr looks up "subscript" → FunctionDecl and emits Call.)

  // Detect get/set accessor bodies.
  Parse::NodeId getter_body = Parse::NodeId::None;
  Parse::NodeId setter_body = Parse::NodeId::None;
  Parse::NodeId pending_code_block = Parse::NodeId::None;
  bool has_explicit = false;
  if (accessor_block.has_value()) {
    for (auto ac : context.children_source_order(accessor_block)) {
      auto ack = context.node_kind(ac);
      if (ack == Parse::NodeKind::AccessorBlockStart ||
          ack == Parse::NodeKind::CodeBlockStart) continue;
      if (ack == Parse::NodeKind::CodeBlock) {
        pending_code_block = ac;
      } else if (ack == Parse::NodeKind::GetAccessor) {
        has_explicit = true;
        if (pending_code_block.has_value()) {
          getter_body = pending_code_block;
          pending_code_block = Parse::NodeId::None;
        }
      } else if (ack == Parse::NodeKind::SetAccessor) {
        has_explicit = true;
        if (pending_code_block.has_value()) {
          setter_body = pending_code_block;
          pending_code_block = Parse::NodeId::None;
        }
      }
    }
    if (!has_explicit) getter_body = accessor_block;
  }

  // Build getter body.
  if (getter_body.has_value()) {
    context.SetCurrentFunction(getter_fn_id);
    context.SetCurrentType(enclosing_type_id);
    auto g_scope_id = context.name_scopes().Add(
        getter_decl_id, getter_mangled_id, context.CurrentScopeId());
    context.PushScope(g_scope_id);
    context.AddNameToScope(self_name_id, getter_self_id);
    for (size_t i = 0; i < params.size(); ++i) {
      context.AddNameToScope(params[i].first, getter_param_ids[i + 1]);
    }
    auto g_block_id = context.PushInstBlock();
    context.functions().Get(getter_fn_id).body_block_ids.push_back(g_block_id);
    HandleCodeBlock(context, getter_body);
    context.PopInstBlock();
    context.ClearCurrentFunction();
    context.ClearCurrentType();
    context.PopScope();
  }

  // Build setter if present.
  if (setter_body.has_value()) {
    sub_name_storage->push_back(std::string(type_name) + ".subscript.set");
    auto setter_mangled_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(sub_name_storage->back()));
    sub_name_storage->push_back("subscript.set");
    auto setter_lookup_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(sub_name_storage->back()));

    auto setter_self_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::InoutParam{.type_id = self_type_id,
                          .index = SemIR::CallParamIndex(0),
                          .name_id = self_name_id}));

    llvm::SmallVector<SemIR::InstId> setter_param_ids = {setter_self_id};
    for (size_t i = 0; i < params.size(); ++i) {
      auto pi = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::ValueParam{.type_id = params[i].second,
                            .index = SemIR::CallParamIndex(static_cast<int32_t>(i + 1)),
                            .pretty_name_id = params[i].first}));
      setter_param_ids.push_back(pi);
    }
    auto nv_ident = context.identifiers().Add("newValue");
    auto nv_name = SemIR::NameId::ForIdentifier(nv_ident);
    auto nv_param = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ValueParam{.type_id = return_type_id,
                          .index = SemIR::CallParamIndex(
                              static_cast<int32_t>(params.size() + 1)),
                          .pretty_name_id = nv_name}));
    setter_param_ids.push_back(nv_param);

    SemIR::Function setter_fn;
    setter_fn.name_id = setter_mangled_id;
    setter_fn.parent_scope_id = context.CurrentScopeId();
    setter_fn.is_mutating = true;
    auto s_params_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        s_params_id, llvm::ArrayRef<SemIR::InstId>(setter_param_ids));
    setter_fn.call_params_id = s_params_id;
    auto s_decl_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(s_decl_id, llvm::ArrayRef<SemIR::InstId>());
    setter_fn.decl_block_id = s_decl_id;
    auto setter_fn_id = context.functions().Add(setter_fn);

    auto setter_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId(node_id),
        SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                            .function_id = setter_fn_id,
                            .decl_block_id = s_decl_id}));
    context.AddNameToScope(setter_lookup_id, setter_decl_id);

    context.SetCurrentFunction(setter_fn_id);
    context.SetCurrentType(enclosing_type_id);
    auto s_scope_id = context.name_scopes().Add(
        setter_decl_id, setter_mangled_id, context.CurrentScopeId());
    context.PushScope(s_scope_id);
    context.AddNameToScope(self_name_id, setter_self_id);
    for (size_t i = 0; i < params.size(); ++i) {
      context.AddNameToScope(params[i].first, setter_param_ids[i + 1]);
    }
    context.AddNameToScope(nv_name, nv_param);
    auto s_block_id = context.PushInstBlock();
    context.functions().Get(setter_fn_id).body_block_ids.push_back(s_block_id);
    HandleCodeBlock(context, setter_body);
    context.PopInstBlock();
    context.ClearCurrentFunction();
    context.ClearCurrentType();
    context.PopScope();
  }
}

// Generates a custom init function for an `init(...)` declaration.
// The init function:
//   - Has the explicit parameters (no self param)
//   - Has return type = the struct type
//   - In its body, `self` is a VarStorage (alloca'd struct)
//   - At the end, auto-returns the initialized self value
static auto HandleInitDefinition(Context& context,
                                 Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // Find InitDefinitionStart (params) and CodeBlock (body).
  Parse::NodeId sig_node_id = Parse::NodeId::None;
  Parse::NodeId body_code_block = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::InitDefinitionStart) {
      sig_node_id = child;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_code_block = child;
    }
  }

  if (!sig_node_id.has_value()) return;

  // Extract parameters from InitDefinitionStart.
  // Structure: InitIntroducer (leaf) + ExplicitParamList + InitDefinitionStart
  struct InitParam {
    SemIR::NameId name_id;
    SemIR::TypeId type_id;
  };
  llvm::SmallVector<InitParam> params;

  for (auto sc : context.children_source_order(sig_node_id)) {
    if (context.node_kind(sc) == Parse::NodeKind::ExplicitParamList) {
      for (auto pc : context.children_source_order(sc)) {
        if (context.node_kind(pc) == Parse::NodeKind::FunctionParam) {
          SemIR::NameId param_name_id = SemIR::NameId::None;
          SemIR::TypeId param_type_id = SemIR::ErrorInst::TypeId;
          for (auto fpc : context.children_source_order(pc)) {
            auto fpc_kind = context.node_kind(fpc);
            if (fpc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                fpc_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
              auto text = context.token_text(context.node_token(fpc));
              param_name_id = SemIR::NameId::ForIdentifier(
                  context.identifiers().Add(text));
            } else if (fpc_kind == Parse::NodeKind::TypeAnnotation ||
                       fpc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
              param_type_id = HandleTypeExpr(context, fpc);
            }
          }
          if (!param_name_id.has_value()) {
            auto text = context.token_text(context.node_token(pc));
            param_name_id = SemIR::NameId::ForIdentifier(
                context.identifiers().Add(text));
          }
          params.push_back({param_name_id, param_type_id});
        }
      }
    }
  }

  // Get the enclosing struct/class type.
  auto enclosing_type_id = context.CurrentTypeInstId();
  if (!enclosing_type_id.has_value()) return;

  // Build mangled name "TypeName.init".
  auto type_inst = context.insts().Get(enclosing_type_id);
  llvm::StringRef type_name;
  if (auto st = type_inst.TryAs<SemIR::StructType>()) {
    auto& scope = context.name_scopes().Get(st->name_scope_id);
    auto ident_opt = scope.name_id.AsIdentifierId();
    if (ident_opt.has_value()) type_name = context.identifiers().Get(ident_opt);
  } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
    auto& scope = context.name_scopes().Get(ct->name_scope_id);
    auto ident_opt = scope.name_id.AsIdentifierId();
    if (ident_opt.has_value()) type_name = context.identifiers().Get(ident_opt);
  }

  static llvm::SmallVector<std::string>* init_name_storage =
      new llvm::SmallVector<std::string>();
  init_name_storage->push_back(std::string(type_name) + ".init");
  auto mangled_ident_id =
      context.identifiers().Add(init_name_storage->back());
  auto mangled_name_id = SemIR::NameId::ForIdentifier(mangled_ident_id);

  // Return type is the struct type itself.
  auto struct_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(enclosing_type_id));

  // Create ValueParam for each user parameter.
  llvm::SmallVector<SemIR::InstId> param_ids;
  for (size_t i = 0; i < params.size(); ++i) {
    auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ValueParam{
            .type_id = params[i].type_id,
            .index = SemIR::CallParamIndex(static_cast<int32_t>(i)),
            .pretty_name_id = params[i].name_id}));
    param_ids.push_back(param_id);
  }

  // Build SemIR Function.
  SemIR::Function fn;
  fn.name_id = mangled_name_id;
  fn.parent_scope_id = context.CurrentScopeId();
  fn.return_type_inst_id =
      SemIR::TypeInstId::UnsafeMake(enclosing_type_id);  // struct type

  if (!param_ids.empty()) {
    auto params_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        params_block_id, llvm::ArrayRef<SemIR::InstId>(param_ids));
    fn.call_params_id = params_block_id;
  }

  auto decl_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(decl_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());
  fn.decl_block_id = decl_block_id;

  auto function_id = context.functions().Add(fn);

  // Emit FunctionDecl instruction.
  auto fn_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId(node_id),
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = function_id,
                          .decl_block_id = decl_block_id}));

  // Register under "init" in the struct scope so HandleCallExpr can find it.
  auto init_ident_id = context.identifiers().Add("init");
  auto init_name_id = SemIR::NameId::ForIdentifier(init_ident_id);
  context.AddNameToScope(init_name_id, fn_decl_id);

  // Set up the init function body.
  context.SetCurrentFunction(function_id);

  auto body_scope_id = context.name_scopes().Add(
      fn_decl_id, mangled_name_id, context.CurrentScopeId());
  context.PushScope(body_scope_id);

  // Register user params in body scope.
  for (size_t i = 0; i < params.size(); ++i) {
    context.AddNameToScope(params[i].name_id, param_ids[i]);
  }

  // Push the entry block first.
  auto body_block_id = context.PushInstBlock();
  context.functions().Get(function_id).body_block_ids.push_back(body_block_id);

  // Add VarStorage for `self` as the first instruction in the body block.
  auto self_var_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::VarStorage{.type_id = struct_type_id,
                        .pattern_id = SemIR::AbsoluteInstId(
                            SemIR::InstId::None)}));

  // Register `self` in the body scope (points to VarStorage alloca).
  auto self_ident_id = context.identifiers().Add("self");
  auto self_name_id = SemIR::NameId::ForIdentifier(self_ident_id);
  context.AddNameToScope(self_name_id, self_var_id);

  // Process body statements.
  if (body_code_block.has_value()) {
    HandleCodeBlock(context, body_code_block);
  }

  // Auto-return: load self and return the initialized struct.
  if (!context.IsCurrentBlockTerminated()) {
    auto self_load_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::NameRef{.type_id = struct_type_id,
                       .name_id = self_name_id,
                       .value_id = self_var_id}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ReturnExpr{
            .expr_id = self_load_id,
            .dest_id = SemIR::DestInstId(SemIR::InstId::None)}));
  }

  context.PopInstBlock();
  context.ClearCurrentFunction();
  context.PopScope();
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
      bool next_is_static = false;
      bool next_is_mutating = false;
      for (auto bc : block_children) {
        auto bc_kind = context.node_kind(bc);
        if (bc_kind == Parse::NodeKind::CodeBlockStart) continue;

        // StaticModifier / MutatingModifier are siblings of FunctionDefinition.
        // Track them so the next FunctionDefinition can use them.
        if (bc_kind == Parse::NodeKind::StaticModifier) {
          next_is_static = true;
          continue;
        }
        if (bc_kind == Parse::NodeKind::MutatingModifier) {
          next_is_mutating = true;
          continue;
        }

        // Reset modifier flags after consuming them for a function.
        bool this_is_static = next_is_static;
        bool this_is_mutating = next_is_mutating;
        next_is_static = false;
        next_is_mutating = false;

        if (bc_kind == Parse::NodeKind::VariableDecl) {
          // Check for computed property (has AccessorBlock child).
          if (IsComputedProperty(context, bc)) {
            // Extract the name, type, and body from the VariableDecl children.
            Parse::NodeId name_node = Parse::NodeId::None;
            Parse::NodeId type_node = Parse::NodeId::None;
            Parse::NodeId accessor_block = Parse::NodeId::None;
            auto var_children = context.children_source_order(bc);
            for (auto vc : var_children) {
              auto vc_kind = context.node_kind(vc);
              if (vc_kind == Parse::NodeKind::IdentifierPattern) {
                name_node = vc;
              } else if (vc_kind == Parse::NodeKind::TypeAnnotation ||
                         vc_kind.category().HasAnyOf(
                             Parse::NodeCategory::Type)) {
                if (!type_node.has_value()) type_node = vc;
              } else if (vc_kind == Parse::NodeKind::AccessorBlock) {
                accessor_block = vc;
              }
            }
            if (name_node.has_value() && accessor_block.has_value()) {
              GenerateComputedPropertyGetter(
                  context, name_node, type_node, accessor_block);
            }
          }
          // Stored fields are already registered as StructField in the
          // CollectFieldDecls pass above; skip here.
          continue;
        }

        // Custom initializer.
        if (bc_kind == Parse::NodeKind::InitDefinition) {
          HandleInitDefinition(context, bc);
          continue;
        }

        // FunctionDefinition: pass static and mutating hints.
        if (bc_kind == Parse::NodeKind::FunctionDefinition) {
          HandleFunctionDefinition(context, bc, this_is_static, this_is_mutating);
          continue;
        }

        // M62: Nested type definitions inside a type body.
        if (bc_kind == Parse::NodeKind::StructDefinition ||
            bc_kind == Parse::NodeKind::ClassDefinition ||
            bc_kind == Parse::NodeKind::EnumDefinition) {
          HandleStatement(context, bc);
          continue;
        }

        // M64: Subscript declarations inside a type body.
        if (bc_kind == Parse::NodeKind::SubscriptDefinition) {
          HandleSubscriptDefinition(context, bc);
          continue;
        }

        if (bc_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                        Parse::NodeCategory::Decl)) {
          HandleStatement(context, bc);
        }
      }
      continue;
    }

    // InitDefinition as a direct child (not inside CodeBlock).
    if (child_kind == Parse::NodeKind::InitDefinition) {
      HandleInitDefinition(context, child);
      continue;
    }

    // M62: Nested type definitions as direct children.
    if (child_kind == Parse::NodeKind::StructDefinition ||
        child_kind == Parse::NodeKind::ClassDefinition ||
        child_kind == Parse::NodeKind::EnumDefinition) {
      HandleStatement(context, child);
      continue;
    }

    // M64: Subscript declarations as direct children.
    if (child_kind == Parse::NodeKind::SubscriptDefinition) {
      HandleSubscriptDefinition(context, child);
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

  // M69: Detect generic parameters. If present and not specializing, store
  // template and skip field/member processing.
  if (start_node_id.has_value() && !context.is_specializing()) {
    auto generic_params = ExtractTypeGenericParams(context, start_node_id);
    if (!generic_params.empty()) {
      // Create a minimal StructType as placeholder (so name is in scope).
      auto type_scope_id = context.name_scopes().Add(
          SemIR::InstId::None, name_id, context.CurrentScopeId());
      auto struct_type_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
          SemIR::StructType{.type_id = SemIR::TypeType::TypeId,
                            .name_scope_id = type_scope_id}));
      if (name_id.has_value()) {
        context.AddNameToScope(name_id, struct_type_id);
        // Store as generic type template.
        Context::GenericTypeTemplate tmpl;
        tmpl.definition_node_id = node_id;
        tmpl.start_node_id = start_node_id;
        tmpl.generic_param_names.assign(generic_params.begin(), generic_params.end());
        tmpl.original_type_inst_id = struct_type_id;
        context.generic_type_templates().insert({name_id.index, tmpl});
      }
      return;  // Skip body processing for generic template.
    }
  }

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
  // Set the current type so HandleFunctionDefinition can synthesize `self`.
  context.SetCurrentType(struct_type_id);
  HandleTypeMembers(context, children);
  context.ClearCurrentType();
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

  // M69: Detect generic parameters for classes. Same pattern as structs.
  if (start_node_id.has_value() && !context.is_specializing()) {
    auto generic_params = ExtractTypeGenericParams(context, start_node_id);
    if (!generic_params.empty()) {
      auto type_scope_id = context.name_scopes().Add(
          SemIR::InstId::None, name_id, context.CurrentScopeId());
      auto class_type_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
          SemIR::ClassType{.type_id = SemIR::TypeType::TypeId,
                           .name_scope_id = type_scope_id}));
      if (name_id.has_value()) {
        context.AddNameToScope(name_id, class_type_id);
        Context::GenericTypeTemplate tmpl;
        tmpl.definition_node_id = node_id;
        tmpl.start_node_id = start_node_id;
        tmpl.generic_param_names.assign(generic_params.begin(), generic_params.end());
        tmpl.original_type_inst_id = class_type_id;
        context.generic_type_templates().insert({name_id.index, tmpl});
      }
      return;
    }
  }

  // M55: Detect InheritanceClause inside ClassDefinitionStart to find superclass.
  SemIR::NameScopeId superclass_scope_id = SemIR::NameScopeId::None;
  if (start_node_id.has_value()) {
    for (auto sc : context.children_source_order(start_node_id)) {
      if (context.node_kind(sc) == Parse::NodeKind::InheritanceClause) {
        // InheritanceClause children: InheritanceClauseStart + IdentifierType(s).
        for (auto ic : context.children_source_order(sc)) {
          if (context.node_kind(ic) == Parse::NodeKind::IdentifierType) {
            auto tok = context.node_token(ic);
            auto super_text = context.token_text(tok);
            auto super_ident_id = context.identifiers().Lookup(super_text);
            if (super_ident_id.has_value()) {
              auto super_name_id = SemIR::NameId::ForIdentifier(super_ident_id);
              auto super_inst_id = context.LookupName(super_name_id);
              if (super_inst_id.has_value()) {
                auto super_inst = context.insts().Get(super_inst_id);
                if (auto ct = super_inst.TryAs<SemIR::ClassType>()) {
                  superclass_scope_id = ct->name_scope_id;
                }
              }
            }
            break;  // Only handle single inheritance.
          }
        }
        break;
      }
    }
  }

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

  // M55: Copy superclass scope entries (fields + methods) into this class scope.
  // Subclass fields follow after superclass fields.
  int32_t field_index = 0;
  if (superclass_scope_id.has_value()) {
    auto& super_scope = context.name_scopes().Get(superclass_scope_id);
    for (auto& [name_idx, inst_id] : super_scope.names) {
      // Re-register in subclass scope so member lookup finds them.
      // Use the InstId directly — the fields are at their original indices.
      context.AddNameToScope(SemIR::NameId(name_idx), inst_id);
      // Track max field index for new subclass fields.
      auto inst = context.insts().Get(inst_id);
      if (auto sf = inst.TryAs<SemIR::StructField>()) {
        if (sf->index.index + 1 > field_index) {
          field_index = sf->index.index + 1;
        }
      }
    }
  }

  for (auto& fi : field_infos) {
    auto field_inst_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(fi.node_id),
        SemIR::StructField{.type_id = fi.type_id,
                           .name_id = fi.name_id,
                           .index = SemIR::ElementIndex(field_index)}));
    context.AddNameToScope(fi.name_id, field_inst_id);
    ++field_index;
  }

  context.SetCurrentType(class_type_id);
  HandleTypeMembers(context, children);
  context.ClearCurrentType();
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

  // M71: Detect generic parameters. If present and not specializing, store
  // template and skip case processing.
  if (start_node_id.has_value() && !context.is_specializing()) {
    auto generic_params = ExtractTypeGenericParams(context, start_node_id);
    if (!generic_params.empty()) {
      auto type_scope_id = context.name_scopes().Add(
          SemIR::InstId::None, name_id, context.CurrentScopeId());
      auto cases_block_id = context.inst_blocks().AddPlaceholder();
      auto enum_type_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
          SemIR::EnumDecl{.type_id = SemIR::TypeType::TypeId,
                          .name_scope_id = type_scope_id,
                          .cases_id = cases_block_id}));
      if (name_id.has_value()) {
        context.AddNameToScope(name_id, enum_type_id);
        Context::GenericTypeTemplate tmpl;
        tmpl.definition_node_id = node_id;
        tmpl.start_node_id = start_node_id;
        tmpl.generic_param_names.assign(generic_params.begin(), generic_params.end());
        tmpl.original_type_inst_id = enum_type_id;
        context.generic_type_templates().insert({name_id.index, tmpl});
      }
      context.inst_blocks().ReplacePlaceholder(
          cases_block_id, llvm::ArrayRef<SemIR::InstId>());
      return;  // Skip body processing for generic template.
    }
  }

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
        // For enum cases with associated values, the parser emits IdentifierType
        // BEFORE IdentifierNameNotBeforeParams so that EnumCaseElement.child_count=1
        // captures the name as its child. The IdentifierType ends up as a direct
        // child of EnumCaseDecl (not EnumCaseElement). We collect it here.
        SemIR::TypeId pending_payload_type = SemIR::TypeId::None;
        for (auto cc : case_children) {
          auto cc_kind = context.node_kind(cc);
          // IdentifierType at EnumCaseDecl level = associated value type
          // (emitted before the name in the parse tree due to ordering).
          if (cc_kind == Parse::NodeKind::IdentifierType ||
              cc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
            if (!pending_payload_type.has_value()) {
              pending_payload_type = HandleTypeExpr(context, cc);
            }
          }
          if (cc_kind == Parse::NodeKind::EnumCaseElement) {
            auto elem_children = context.children_source_order(cc);

            // Collect name from the element's single child (IdentifierNameNotBeforeParams).
            llvm::StringRef case_text;
            Parse::NodeId case_name_node = Parse::NodeId::None;
            SemIR::TypeId payload_type_id = pending_payload_type;
            pending_payload_type = SemIR::TypeId::None;  // consume

            for (auto ec : elem_children) {
              auto ec_kind = context.node_kind(ec);
              if (ec_kind == Parse::NodeKind::IdentifierNameNotBeforeParams) {
                case_name_node = ec;
                case_text = context.token_text(context.node_token(ec));
              } else if (ec_kind == Parse::NodeKind::IdentifierType ||
                         ec_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
                // Fallback: type directly in elem_children (alternative ordering).
                if (!payload_type_id.has_value()) {
                  payload_type_id = HandleTypeExpr(context, ec);
                }
              }
            }

            if (case_text.empty()) continue;

            auto ident_id = context.identifiers().Add(case_text);
            auto case_name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto loc = SemIR::LocId(
                case_name_node.has_value() ? case_name_node : cc);

            SemIR::InstId case_id = SemIR::InstId::None;
            if (payload_type_id.has_value()) {
              // Case with associated value — emit EnumCaseWithPayload.
              auto payload_type_inst_id =
                  context.types().GetTypeInstId(payload_type_id);
              case_id = context.AddInst(SemIR::LocIdAndInst(
                  loc,
                  SemIR::EnumCaseWithPayload{
                      .type_id = enum_value_type_id,
                      .discriminant = SemIR::ElementIndex(discriminant),
                      .payload_type_id = SemIR::InstId(
                          payload_type_inst_id.index)}));
            } else {
              // Simple case (no payload) — emit EnumCase.
              case_id = context.AddInst(SemIR::LocIdAndInst(
                  loc,
                  SemIR::EnumCase{
                      .type_id = enum_value_type_id,
                      .name_id = case_name_id,
                      .discriminant = SemIR::ElementIndex(discriminant)}));
            }

            case_inst_ids.push_back(case_id);
            context.AddNameToScope(case_name_id, case_id);
            ++discriminant;
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

  // M59: Protocols are purely static-dispatch conformance markers.
  // Register the protocol name in scope but do NOT process the body —
  // abstract method declarations have no body and would generate invalid
  // SIL functions if lowered. At call sites, static dispatch finds the
  // concrete struct's method directly via the struct's own scope.
  auto proto_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::StructType{.type_id = SemIR::TypeType::TypeId,
                        .name_scope_id = type_scope_id}));

  if (name_id.has_value()) {
    context.AddNameToScope(name_id, proto_id);
  }
  // Intentionally do not call HandleTypeMembers — protocol body is abstract.
}

auto HandleExtensionDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  auto children = context.children_source_order(node_id);

  // Find the extended type name.
  // Parse tree: ExtensionDefinition → [ExtensionDefinitionStart, CodeBlock]
  //             ExtensionDefinitionStart → [ExtensionIntroducer, IdentifierType]
  // IdentifierType is inside ExtensionDefinitionStart, not a direct child.
  llvm::StringRef type_name;
  for (auto child : children) {
    if (context.node_kind(child) == Parse::NodeKind::ExtensionDefinitionStart) {
      // Look inside the start node for IdentifierType.
      for (auto sc : context.children_source_order(child)) {
        if (context.node_kind(sc) == Parse::NodeKind::IdentifierType) {
          type_name = context.token_text(context.node_token(sc));
          break;
        }
      }
      break;
    }
  }

  if (type_name.empty()) {
    // Fallback: process members in current scope.
    for (auto child : children) {
      auto child_kind = context.node_kind(child);
      if (child_kind == Parse::NodeKind::ExtensionDefinitionStart) continue;
      if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                         Parse::NodeCategory::Decl)) {
        HandleStatement(context, child);
      }
    }
    return;
  }

  // Look up the extended type by name in the current scope.
  auto ident_id = context.identifiers().Lookup(type_name);
  if (!ident_id.has_value()) return;  // Unknown type — skip.

  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto type_inst_id = context.LookupName(name_id);
  if (!type_inst_id.has_value()) return;  // Not in scope — skip.

  // Determine the type's name scope.
  auto type_inst = context.insts().Get(type_inst_id);
  SemIR::NameScopeId type_scope_id = SemIR::NameScopeId::None;
  if (auto st = type_inst.TryAs<SemIR::StructType>()) {
    type_scope_id = st->name_scope_id;
  } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
    type_scope_id = ct->name_scope_id;
  } else if (auto ed = type_inst.TryAs<SemIR::EnumDecl>()) {
    type_scope_id = ed->name_scope_id;
  }

  if (!type_scope_id.has_value()) {
    // Not a known composite type — skip.
    return;
  }

  // Push the type's existing scope so newly-registered members land in it.
  context.PushScope(type_scope_id);
  // Set enclosing type so HandleFunctionDefinition synthesizes `self` correctly.
  context.SetCurrentType(type_inst_id);

  // Process members — reuses existing logic for FunctionDefinition, etc.
  HandleTypeMembers(context, children);

  context.ClearCurrentType();
  context.PopScope();
}

auto HandleTypealiasDecl(Context& context, Parse::NodeId node_id) -> void {
  SemIR::NameId alias_name_id = SemIR::NameId::None;
  SemIR::TypeId aliased_type_id = SemIR::ErrorInst::TypeId;

  auto children = context.children_source_order(node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
        child_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      alias_name_id = SemIR::NameId::ForIdentifier(ident_id);
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
      aliased_type_id = HandleTypeExpr(context, child);
    }
  }

  if (alias_name_id.has_value() &&
      aliased_type_id != SemIR::ErrorInst::TypeId) {
    context.typealias_map().insert({alias_name_id.index, aliased_type_id});
  }
}

}  // namespace TinySwift::Check
