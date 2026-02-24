// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_function.h"

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Processes the function signature (name, params, return type) from a
// FunctionDefinitionStart or FunctionDecl node.
struct FunctionSignature {
  SemIR::NameId name_id = SemIR::NameId::None;
  llvm::SmallVector<std::pair<SemIR::NameId, SemIR::TypeId>> params;
  // Default value parse node for each param (Parse::NodeId::None if no default).
  llvm::SmallVector<Parse::NodeId> param_defaults;
  // Whether each param is inout (M40).
  llvm::SmallVector<bool> param_is_inout;
  SemIR::TypeId return_type_id = SemIR::TypeId::None;
  Parse::NodeId name_node_id = Parse::NodeId::None;
  // Whether the function is declared with `throws` (M41).
  bool is_throwing = false;
};

auto ExtractFunctionSignature(Context& context, Parse::NodeId sig_node_id)
    -> FunctionSignature {
  FunctionSignature sig;
  auto children = context.children_source_order(sig_node_id);

  // Type nodes that appear as DIRECT children of FunctionDefinitionStart
  // between ExplicitParamList and ReturnType are element types for a tuple
  // return type like `-> (Int, Int)`. In the parse tree, `(Int, Int)` stores
  // TupleType as the only child of ReturnType (a leaf marker), while the
  // element types Int, Int are siblings of ReturnType within FunctionDefinitionStart.
  llvm::SmallVector<Parse::NodeId> pre_return_type_nodes;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::FunctionIntroducer) {
      continue;
    }

    if (child_kind == Parse::NodeKind::IdentifierNameBeforeParams ||
        child_kind == Parse::NodeKind::IdentifierNameNotBeforeParams) {
      sig.name_node_id = child;
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      sig.name_id = SemIR::NameId::ForIdentifier(ident_id);
      continue;
    }

    if (child_kind == Parse::NodeKind::ExplicitParamList) {
      // Process parameters in source order.
      // NOTE: For `_ x: inout Int`, the parse tree structure is:
      //   ExplicitParamList
      //     ExplicitParamListStart
      //     ExternalParamName("_")   <- sibling
      //     IdentifierPattern("x")   <- sibling of FunctionParam (NOT child)
      //     FunctionParam (children: InoutMarker, TypeAnnotation)
      // For `_ x: Int`, the structure is:
      //   ExplicitParamList
      //     ExplicitParamListStart
      //     ExternalParamName("_")   <- sibling
      //     FunctionParam (children: IdentifierPattern, TypeAnnotation)
      // So we track a pending sibling IdentifierPattern for the next FunctionParam.
      auto param_children = context.children_source_order(child);
      SemIR::NameId pending_sibling_name = SemIR::NameId::None;

      for (auto pc : param_children) {
        auto pc_kind = context.node_kind(pc);

        // Track sibling IdentifierPattern as the pending internal param name.
        if (pc_kind == Parse::NodeKind::IdentifierPattern) {
          auto token = context.node_token(pc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_sibling_name = SemIR::NameId::ForIdentifier(ident_id);
          continue;
        }

        if (pc_kind == Parse::NodeKind::FunctionParam) {
          // FunctionParam has children: InoutMarker? + TypeAnnotation,
          // and optional IdentifierPattern as a CHILD (for non-inout params)
          // or as a SIBLING (captured in pending_sibling_name, for inout params).
          SemIR::NameId param_name_id = pending_sibling_name;
          SemIR::TypeId param_type_id = SemIR::ErrorInst::TypeId;
          Parse::NodeId default_node = Parse::NodeId::None;
          bool is_inout = false;  // M40: track inout modifier
          pending_sibling_name = SemIR::NameId::None;  // consume pending name

          auto fp_children = context.children_source_order(pc);
          for (auto fpc : fp_children) {
            auto fpc_kind = context.node_kind(fpc);
            if (fpc_kind == Parse::NodeKind::InoutMarker) {
              // M40: InoutMarker is a direct child of FunctionParam for
              // `_ x: inout T` (because FunctionParam.child_count=2 takes
              // InoutMarker + TypeAnnotation, leaving IdentifierPattern as sibling).
              is_inout = true;
            } else if (fpc_kind == Parse::NodeKind::ExternalParamName) {
              // External label (`_` or `label`) — skip for body name.
            } else if (fpc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                       fpc_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
              // Internal parameter name (single-name case: `name: Type`).
              auto token = context.node_token(fpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (fpc_kind == Parse::NodeKind::IdentifierPattern) {
              // Internal name as direct child of FunctionParam
              // (for non-inout params like `_ x: Int`).
              auto token = context.node_token(fpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (fpc_kind == Parse::NodeKind::TypeAnnotation ||
                       fpc_kind.category().HasAnyOf(
                           Parse::NodeCategory::Type)) {
              param_type_id = HandleTypeExpr(context, fpc);
            } else if (fpc_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
              // Trailing expr child is the default value expression.
              default_node = fpc;
            }
          }

          // Fallback: use the FunctionParam's own token as the param name.
          if (!param_name_id.has_value()) {
            auto token = context.node_token(pc);
            auto text = context.token_text(token);
            if (text != "_") {  // skip wildcard
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            }
          }

          sig.params.push_back({param_name_id, param_type_id});
          sig.param_defaults.push_back(default_node);
          sig.param_is_inout.push_back(is_inout);
        }
      }
      continue;
    }

    if (child_kind == Parse::NodeKind::ReturnType) {
      auto rt_children = context.children_source_order(child);

      // Check if the ReturnType's child is a TupleType node (tuple return).
      // If so, `pre_return_type_nodes` holds the element types.
      bool has_tuple_child = false;
      for (auto rtc : rt_children) {
        if (context.node_kind(rtc) == Parse::NodeKind::TupleType) {
          has_tuple_child = true;
          break;
        }
      }

      if (has_tuple_child && !pre_return_type_nodes.empty()) {
        // Build a TupleType with properly populated element_types_id.
        // Each pre_return_type_node is an element type (e.g., IdentifierType(Int)).
        llvm::SmallVector<SemIR::InstId> elem_type_insts;
        for (auto tn : pre_return_type_nodes) {
          auto et_id = HandleTypeExpr(context, tn);
          auto et_inst_id = context.types().GetTypeInstId(et_id);
          if (et_inst_id.has_value()) {
            // TypeInstId IS-A InstId (inherits), so store directly.
            elem_type_insts.push_back(et_inst_id);
          }
        }
        auto block = context.inst_blocks().AddPlaceholder();
        context.inst_blocks().ReplacePlaceholder(
            block, llvm::ArrayRef<SemIR::InstId>(elem_type_insts));
        auto tt_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(child),
            SemIR::TupleType{.type_id = SemIR::TypeType::TypeId,
                             .element_types_id = block}));
        sig.return_type_id = SemIR::TypeId::ForTypeConstant(
            SemIR::ConstantId::ForConcreteConstant(tt_id));
      } else {
        // Non-tuple return type: use the first Type child of ReturnType.
        for (auto rtc : rt_children) {
          if (context.node_kind(rtc).category().HasAnyOf(
                  Parse::NodeCategory::Type)) {
            sig.return_type_id = HandleTypeExpr(context, rtc);
            break;
          }
        }
      }
      pre_return_type_nodes.clear();
      continue;
    }

    // Collect type nodes that appear between ExplicitParamList and ReturnType.
    // These are element types for a tuple return type `-> (Int, Int)`.
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
      pre_return_type_nodes.push_back(child);
    }
  }

  return sig;
}

}  // namespace

auto HandleFunctionDefinition(Context& context, Parse::NodeId node_id,
                              bool is_static_hint) -> void {
  // M52: Save outer function context so nested function definitions don't
  // clobber the outer function's state.
  auto outer_function_id = context.CurrentFunctionId();
  // Save and clear deferred blocks for the outer function (M51 × M52 interaction).
  // The nested function gets its own empty defer stack; the outer function's
  // deferred blocks are restored after the nested function is processed.
  auto outer_deferred_blocks = context.GetDeferredBlocks();
  context.ClearDeferredBlocks();

  auto children = context.children_source_order(node_id);

  // First child should be FunctionDefinitionStart.
  // StaticModifier is a SIBLING of FunctionDefinition in the parent CodeBlock,
  // NOT a child — it's detected by HandleTypeMembers and passed via is_static_hint.
  Parse::NodeId sig_node_id = Parse::NodeId::None;
  Parse::NodeId body_code_block = Parse::NodeId::None;
  bool is_static = is_static_hint;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::FunctionDefinitionStart) {
      sig_node_id = child;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_code_block = child;
    }
  }

  if (!sig_node_id.has_value()) {
    return;
  }

  auto sig = ExtractFunctionSignature(context, sig_node_id);

  // Check if we're inside a type (struct/class). If so, either synthesize
  // `self` (instance method) or skip it (static method), and mangle the name.
  auto enclosing_type_id = context.CurrentTypeInstId();
  int param_index_offset = 0;
  SemIR::InstId self_param_id = SemIR::InstId::None;
  SemIR::NameId self_name_id = SemIR::NameId::None;
  SemIR::NameId original_name_id = sig.name_id;

  if (enclosing_type_id.has_value()) {
    // Determine the type name for mangling.
    auto type_inst = context.insts().Get(enclosing_type_id);
    llvm::StringRef type_name;
    if (auto st = type_inst.TryAs<SemIR::StructType>()) {
      auto& scope = context.name_scopes().Get(st->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) {
        type_name = context.identifiers().Get(ident_opt);
      }
    } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
      auto& scope = context.name_scopes().Get(ct->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) {
        type_name = context.identifiers().Get(ident_opt);
      }
    }

    if (!type_name.empty() && sig.name_id.has_value() &&
        sig.name_id.AsIdentifierId().has_value()) {
      auto method_name = context.identifiers().Get(sig.name_id.AsIdentifierId());
      // Build "TypeName.methodName" using persistent heap storage so the
      // StringRef remains valid for the lifetime of the identifier store.
      static llvm::SmallVector<std::string>* method_name_storage =
          new llvm::SmallVector<std::string>();
      method_name_storage->push_back(
          std::string(type_name) + "." + std::string(method_name));
      auto mangled_ident_id =
          context.identifiers().Add(method_name_storage->back());
      sig.name_id = SemIR::NameId::ForIdentifier(mangled_ident_id);
    }

    // For instance methods only: synthesize a `self` parameter at index 0.
    // Static methods do NOT get a self parameter.
    if (!is_static) {
      auto self_type_id = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(enclosing_type_id));
      auto self_ident_id = context.identifiers().Add("self");
      self_name_id = SemIR::NameId::ForIdentifier(self_ident_id);

      self_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                     : node_id),
          SemIR::ValueParam{.type_id = self_type_id,
                            .index = SemIR::CallParamIndex(0),
                            .pretty_name_id = self_name_id}));
      param_index_offset = 1;
    }
  }

  // M52: If this is a nested function (inside another function, not inside a type),
  // mangle the name for LLVM symbol uniqueness to avoid conflicts between nested
  // functions with the same name in different outer functions.
  if (outer_function_id.has_value() && !enclosing_type_id.has_value() &&
      sig.name_id.has_value() && sig.name_id.AsIdentifierId().has_value()) {
    static int nested_counter = 0;
    static auto* nested_name_storage = new llvm::SmallVector<std::string>();
    auto orig = context.identifiers().Get(sig.name_id.AsIdentifierId());
    nested_name_storage->push_back(
        "__nested_" + std::to_string(nested_counter++) + "_" + orig.str());
    sig.name_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(nested_name_storage->back()));
    // original_name_id stays unmangled and is registered in scope for
    // call-site lookup.
  }

  // Create a new function in the function store.
  SemIR::Function fn;
  fn.name_id = sig.name_id;  // mangled name if inside a type/nested, else original
  fn.parent_scope_id = context.CurrentScopeId();
  fn.is_static = is_static;
  fn.is_throwing = sig.is_throwing;
  fn.param_default_nodes = sig.param_defaults;

  // Create parameter patterns and params.
  llvm::SmallVector<SemIR::InstId> param_pattern_ids;
  llvm::SmallVector<SemIR::InstId> param_ids;

  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    auto param_index =
        SemIR::CallParamIndex(static_cast<int32_t>(i) + param_index_offset);

    auto entity_name_id = context.entity_names().Add(
        {.name_id = param_name_id, .parent_scope_id = context.CurrentScopeId()});

    // Create ValueBindingPattern for each param.
    auto pattern_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                    : node_id),
        SemIR::ValueBindingPattern{
            .type_id = param_type_id, .entity_name_id = entity_name_id}));

    auto param_pattern_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                    : node_id),
        SemIR::ValueParamPattern{.type_id = param_type_id,
                                 .subpattern_id = pattern_id,
                                 .index = param_index}));
    param_pattern_ids.push_back(param_pattern_id);

    // Create ValueParam or InoutParam (M40).
    SemIR::InstId param_id = SemIR::InstId::None;
    bool is_inout = (i < sig.param_is_inout.size()) && sig.param_is_inout[i];
    if (is_inout) {
      param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                      : node_id),
          SemIR::InoutParam{.type_id = param_type_id,
                            .index = param_index,
                            .name_id = param_name_id}));
    } else {
      param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                      : node_id),
          SemIR::ValueParam{.type_id = param_type_id,
                            .index = param_index,
                            .pretty_name_id = param_name_id}));
    }
    param_ids.push_back(param_id);
  }

  // Store param patterns and params as blocks. Prepend `self` if in a method.
  {
    llvm::SmallVector<SemIR::InstId> all_param_ids;
    if (self_param_id.has_value()) {
      all_param_ids.push_back(self_param_id);
    }
    all_param_ids.append(param_ids.begin(), param_ids.end());

    if (!all_param_ids.empty()) {
      auto params_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          params_block_id, llvm::ArrayRef<SemIR::InstId>(all_param_ids));
      fn.call_params_id = params_block_id;
    }

    if (!param_pattern_ids.empty()) {
      auto patterns_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          patterns_block_id,
          llvm::ArrayRef<SemIR::InstId>(param_pattern_ids));
      fn.call_param_patterns_id = patterns_block_id;
    }
  }

  // Set return type.
  if (sig.return_type_id.has_value()) {
    fn.return_type_inst_id = context.types().GetTypeInstId(sig.return_type_id);
  }

  // Decl block.
  auto decl_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(decl_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());
  fn.decl_block_id = decl_block_id;

  auto function_id = context.functions().Add(fn);

  // Emit FunctionDecl instruction.
  auto fn_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id : node_id),
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = function_id,
                          .decl_block_id = decl_block_id}));

  // Register function name in scope. Use the ORIGINAL (unmangled) name so
  // that `p.method` lookup in HandleMemberAccessExpr works correctly.
  if (original_name_id.has_value()) {
    context.AddNameToScope(original_name_id, fn_decl_id);
  }

  // Track the current function for body block registration.
  context.SetCurrentFunction(function_id);

  // Now process the body.
  // Push a new scope for the function body.
  auto body_scope_id = context.name_scopes().Add(
      fn_decl_id, sig.name_id, context.CurrentScopeId());
  context.PushScope(body_scope_id);

  // If this is a method, add `self` to the function body scope.
  if (self_param_id.has_value() && self_name_id.has_value()) {
    context.AddNameToScope(self_name_id, self_param_id);
  }

  // Add regular parameters to the function's scope.
  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    context.AddNameToScope(param_name_id, param_ids[i]);
  }

  // Push a new inst block for the function body.
  // Add the main body block to body_block_ids FIRST so it's always bb0 (entry).
  auto body_block_id = context.PushInstBlock();
  context.functions().Get(function_id).body_block_ids.push_back(body_block_id);

  // Process body statements via HandleCodeBlock which handles source ordering
  // and condition expression association for if/while/guard.
  if (body_code_block.has_value()) {
    HandleCodeBlock(context, body_code_block);
  }

  context.PopInstBlock();

  // M51: Clear this function's deferred blocks (they have been emitted at each
  // return site already).
  context.ClearDeferredBlocks();

  // M51/M52: Restore outer function's deferred blocks.
  for (auto block : outer_deferred_blocks) {
    context.PushDeferredBlock(block);
  }

  // M52: Restore outer function instead of clearing (supports nested functions).
  context.SetCurrentFunction(outer_function_id);

  context.PopScope();
}

auto HandleFunctionDecl(Context& context, Parse::NodeId node_id) -> void {
  // Forward declaration - extract signature and register name.
  auto sig = ExtractFunctionSignature(context, node_id);

  SemIR::Function fn;
  fn.name_id = sig.name_id;
  fn.parent_scope_id = context.CurrentScopeId();
  fn.param_default_nodes = sig.param_defaults;

  if (sig.return_type_id.has_value()) {
    fn.return_type_inst_id = context.types().GetTypeInstId(sig.return_type_id);
  }

  auto decl_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(decl_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());
  fn.decl_block_id = decl_block_id;

  auto function_id = context.functions().Add(fn);

  auto fn_decl_id = context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
      SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id : node_id),
      SemIR::FunctionDecl{.type_id = SemIR::TypeType::TypeId,
                          .function_id = function_id,
                          .decl_block_id = decl_block_id}));

  if (sig.name_id.has_value()) {
    context.AddNameToScope(sig.name_id, fn_decl_id);
  }
}

}  // namespace TinySwift::Check
