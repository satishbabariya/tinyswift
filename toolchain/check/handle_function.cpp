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
  SemIR::TypeId return_type_id = SemIR::TypeId::None;
  Parse::NodeId name_node_id = Parse::NodeId::None;
};

auto ExtractFunctionSignature(Context& context, Parse::NodeId sig_node_id)
    -> FunctionSignature {
  FunctionSignature sig;
  auto children = context.tree_and_subtrees().children(sig_node_id);

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
    }

    if (child_kind == Parse::NodeKind::ExplicitParamList) {
      // Process parameters.
      auto param_children = context.tree_and_subtrees().children(child);
      for (auto pc : param_children) {
        auto pc_kind = context.node_kind(pc);
        if (pc_kind == Parse::NodeKind::FunctionParam) {
          // FunctionParam has name and type annotation.
          SemIR::NameId param_name_id = SemIR::NameId::None;
          SemIR::TypeId param_type_id = SemIR::ErrorInst::TypeId;

          auto fp_children = context.tree_and_subtrees().children(pc);
          for (auto fpc : fp_children) {
            auto fpc_kind = context.node_kind(fpc);
            if (fpc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                fpc_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
              // This is the parameter name.
              auto token = context.node_token(fpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (fpc_kind == Parse::NodeKind::TypeAnnotation ||
                       fpc_kind.category().HasAnyOf(
                           Parse::NodeCategory::Type)) {
              param_type_id = HandleTypeExpr(context, fpc);
            }
          }

          // The token of the FunctionParam node itself is the parameter name.
          if (!param_name_id.has_value()) {
            auto token = context.node_token(pc);
            auto text = context.token_text(token);
            auto ident_id = context.identifiers().Add(text);
            param_name_id = SemIR::NameId::ForIdentifier(ident_id);
          }

          sig.params.push_back({param_name_id, param_type_id});
        }
      }
    }

    if (child_kind == Parse::NodeKind::ReturnType) {
      auto rt_children = context.tree_and_subtrees().children(child);
      for (auto rtc : rt_children) {
        if (context.node_kind(rtc).category().HasAnyOf(
                Parse::NodeCategory::Type)) {
          sig.return_type_id = HandleTypeExpr(context, rtc);
          break;
        }
      }
    }
  }

  return sig;
}

}  // namespace

auto HandleFunctionDefinition(Context& context, Parse::NodeId node_id)
    -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  // First child should be FunctionDefinitionStart.
  Parse::NodeId sig_node_id = Parse::NodeId::None;
  llvm::SmallVector<Parse::NodeId> body_stmt_ids;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::FunctionDefinitionStart) {
      sig_node_id = child;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                              Parse::NodeCategory::Decl)) {
      body_stmt_ids.push_back(child);
    }
  }

  if (!sig_node_id.has_value()) {
    return;
  }

  auto sig = ExtractFunctionSignature(context, sig_node_id);

  // Create a new function in the function store.
  SemIR::Function fn;
  fn.name_id = sig.name_id;
  fn.parent_scope_id = context.CurrentScopeId();

  // Create parameter patterns and params.
  llvm::SmallVector<SemIR::InstId> param_pattern_ids;
  llvm::SmallVector<SemIR::InstId> param_ids;

  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    auto param_index = SemIR::CallParamIndex(static_cast<int32_t>(i));

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

    // Create ValueParam.
    auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                    : node_id),
        SemIR::ValueParam{.type_id = param_type_id,
                          .index = param_index,
                          .pretty_name_id = param_name_id}));
    param_ids.push_back(param_id);
  }

  // Store param patterns and params as blocks.
  if (!param_pattern_ids.empty()) {
    auto patterns_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        patterns_block_id,
        llvm::ArrayRef<SemIR::InstId>(param_pattern_ids));
    fn.call_param_patterns_id = patterns_block_id;

    auto params_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        params_block_id, llvm::ArrayRef<SemIR::InstId>(param_ids));
    fn.call_params_id = params_block_id;
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

  // Register function name in scope.
  if (sig.name_id.has_value()) {
    context.AddNameToScope(sig.name_id, fn_decl_id);
  }

  // Now process the body.
  // Push a new scope for the function body.
  auto body_scope_id = context.name_scopes().Add(
      fn_decl_id, sig.name_id, context.CurrentScopeId());
  context.PushScope(body_scope_id);

  // Add parameters to the function's scope.
  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    context.AddNameToScope(param_name_id, param_ids[i]);
  }

  // Push a new inst block for the function body.
  context.PushInstBlock();

  // Process body statements.
  for (auto stmt_id : body_stmt_ids) {
    HandleStatement(context, stmt_id);
  }

  auto body_block_id = context.PopInstBlock();

  // Store body block in the function.
  context.functions().Get(function_id).body_block_ids.push_back(body_block_id);

  context.PopScope();
}

auto HandleFunctionDecl(Context& context, Parse::NodeId node_id) -> void {
  // Forward declaration - extract signature and register name.
  auto sig = ExtractFunctionSignature(context, node_id);

  SemIR::Function fn;
  fn.name_id = sig.name_id;
  fn.parent_scope_id = context.CurrentScopeId();

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
