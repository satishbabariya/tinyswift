// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_stmt.h"

#include "toolchain/check/handle_decl.h"
#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Handles a return statement.
auto HandleReturnStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId expr_id = SemIR::InstId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ReturnStatementStart) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      expr_id = HandleExpr(context, child);
    }
  }

  if (expr_id.has_value()) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ReturnExpr{.expr_id = expr_id,
                          .dest_id = SemIR::DestInstId(SemIR::InstId::None)}));
  } else {
    context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
                                        SemIR::Return{}));
  }
}

// Handles an if statement.
auto HandleIfStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId cond_id = SemIR::InstId::None;
  Parse::NodeId then_block_node = Parse::NodeId::None;
  Parse::NodeId else_block_node = Parse::NodeId::None;
  bool found_else = false;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::IfCondition) {
      // IfCondition wraps an expression.
      auto cond_children = context.tree_and_subtrees().children(child);
      for (auto cc : cond_children) {
        if (context.node_kind(cc).category().HasAnyOf(
                Parse::NodeCategory::Expr)) {
          cond_id = HandleExpr(context, cc);
          break;
        }
      }
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      if (!found_else) {
        then_block_node = child;
      } else {
        else_block_node = child;
      }
    } else if (child_kind == Parse::NodeKind::IfStatementElse) {
      found_else = true;
    } else if (child_kind == Parse::NodeKind::IfStatement) {
      // Else-if: treat as nested if in else block.
      else_block_node = child;
    }
  }

  // Emit branch to then block.
  auto then_label = context.PushInstBlock();

  // Emit BranchIf to then block.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(then_label),
                       .cond_id = cond_id}));

  // Process then block.
  if (then_block_node.has_value()) {
    HandleCodeBlock(context, then_block_node);
  }
  context.PopInstBlock();

  // Process else block if present.
  if (else_block_node.has_value()) {
    auto else_label = context.PushInstBlock();
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(else_label)}));

    auto else_kind = context.node_kind(else_block_node);
    if (else_kind == Parse::NodeKind::CodeBlock) {
      HandleCodeBlock(context, else_block_node);
    } else if (else_kind == Parse::NodeKind::IfStatement) {
      HandleIfStatement(context, else_block_node);
    }
    context.PopInstBlock();
  }
}

// Handles a while statement.
auto HandleWhileStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  Parse::NodeId cond_node = Parse::NodeId::None;
  Parse::NodeId body_node = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::WhileCondition) {
      cond_node = child;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_node = child;
    }
  }

  // Create condition block.
  auto cond_label = context.PushInstBlock();

  // Evaluate condition.
  SemIR::InstId cond_id = SemIR::InstId::None;
  if (cond_node.has_value()) {
    auto cond_children = context.tree_and_subtrees().children(cond_node);
    for (auto cc : cond_children) {
      if (context.node_kind(cc).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        cond_id = HandleExpr(context, cc);
        break;
      }
    }
  }

  // Create body block.
  auto body_label = context.PushInstBlock();

  // BranchIf from condition to body.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(body_label),
                       .cond_id = cond_id}));

  // Process body.
  if (body_node.has_value()) {
    HandleCodeBlock(context, body_node);
  }

  // Branch back to condition.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(cond_label)}));

  context.PopInstBlock();  // body
  context.PopInstBlock();  // cond
}

// Handles a guard statement.
auto HandleGuardStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId cond_id = SemIR::InstId::None;
  Parse::NodeId else_block_node = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::GuardCondition) {
      auto cond_children = context.tree_and_subtrees().children(child);
      for (auto cc : cond_children) {
        if (context.node_kind(cc).category().HasAnyOf(
                Parse::NodeCategory::Expr)) {
          cond_id = HandleExpr(context, cc);
          break;
        }
      }
    } else if (child_kind == Parse::NodeKind::GuardStatementElse) {
      // Next sibling after else keyword is the body.
      continue;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      else_block_node = child;
    }
  }

  // Guard: if condition is false, branch to else block (which must diverge).
  auto else_label = context.PushInstBlock();

  // BranchIf with negated condition: branch to else when cond is false.
  // We emit: if (!cond) goto else_block
  // Since we only have BranchIf (branch when true), we negate.
  auto bool_type = context.GetBuiltinType("Bool");
  auto not_cond_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BoolNot{.type_id = bool_type, .operand_id = cond_id}));

  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(else_label),
                       .cond_id = not_cond_id}));

  if (else_block_node.has_value()) {
    HandleCodeBlock(context, else_block_node);
  }
  context.PopInstBlock();
}

// Handles a for-in statement (basic desugaring to while loop).
auto HandleForInStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  // For now, emit a simple while-style loop structure.
  // Full iterator protocol support requires stdlib integration.
  Parse::NodeId body_node = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::CodeBlock) {
      body_node = child;
    }
  }

  // Create a loop structure: condition block + body block.
  auto cond_label = context.PushInstBlock();
  auto body_label = context.PushInstBlock();

  // For now, emit unconditional branch to body (stub).
  auto bool_type = context.GetBuiltinType("Bool");
  auto true_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BoolLiteral{.type_id = bool_type,
                         .value = SemIR::BoolValue::From(true)}));

  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(body_label),
                       .cond_id = true_id}));

  if (body_node.has_value()) {
    HandleCodeBlock(context, body_node);
  }

  // Branch back to condition.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(cond_label)}));

  context.PopInstBlock();  // body
  context.PopInstBlock();  // cond
}

// Handles a switch statement (basic if-else chain desugaring).
auto HandleSwitchStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  // Extract the scrutinee expression.
  SemIR::InstId scrutinee_id = SemIR::InstId::None;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::SwitchIntroducer) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      scrutinee_id = HandleExpr(context, child);
      break;
    }
  }

  // Process each case as a BranchIf chain.
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::SwitchCaseLabel ||
        child_kind == Parse::NodeKind::SwitchDefaultLabel) {
      // Process the code block following this label.
      continue;
    }
    if (child_kind == Parse::NodeKind::CodeBlock) {
      auto case_label = context.PushInstBlock();
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(case_label)}));
      HandleCodeBlock(context, child);
      context.PopInstBlock();
    }
  }
}

// Handles a defer statement.
auto HandleDeferStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  // For now, just process the deferred body inline at the current position.
  // Proper defer semantics (execute at scope exit) will be implemented in SIL.
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::DeferStatementStart) {
      continue;
    }
    if (child_kind == Parse::NodeKind::CodeBlock) {
      HandleCodeBlock(context, child);
    }
  }
}

}  // namespace

auto HandleCodeBlock(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.tree_and_subtrees().children(node_id);

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::CodeBlockStart) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                       Parse::NodeCategory::Decl)) {
      HandleStatement(context, child);
    }
  }
}

auto HandleStatement(Context& context, Parse::NodeId node_id) -> void {
  if (!node_id.has_value()) {
    return;
  }

  auto kind = context.node_kind(node_id);

  // Declarations.
  if (kind == Parse::NodeKind::LetDecl) {
    HandleLetDecl(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::VariableDecl) {
    HandleVariableDecl(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::FunctionDefinition) {
    HandleFunctionDefinition(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::FunctionDecl) {
    HandleFunctionDecl(context, node_id);
    return;
  }

  // Type declarations.
  if (kind == Parse::NodeKind::StructDefinition) {
    HandleStructDefinition(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::ClassDefinition) {
    HandleClassDefinition(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::EnumDefinition) {
    HandleEnumDefinition(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::ProtocolDefinition) {
    HandleProtocolDefinition(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::ExtensionDefinition) {
    HandleExtensionDefinition(context, node_id);
    return;
  }

  // Statements.
  if (kind == Parse::NodeKind::ReturnStatement) {
    HandleReturnStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::IfStatement) {
    HandleIfStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::WhileStatement) {
    HandleWhileStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::GuardStatement) {
    HandleGuardStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::ForInStatement) {
    HandleForInStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::SwitchStatement) {
    HandleSwitchStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::DeferStatement) {
    HandleDeferStatement(context, node_id);
    return;
  }

  // Expression statement.
  if (kind == Parse::NodeKind::ExprStatement) {
    auto children = context.tree_and_subtrees().children(node_id);
    for (auto child : children) {
      if (context.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        HandleExpr(context, child);
      }
    }
    return;
  }

  // Code block.
  if (kind == Parse::NodeKind::CodeBlock) {
    HandleCodeBlock(context, node_id);
    return;
  }

  // Import.
  if (kind == Parse::NodeKind::ImportDecl) {
    // Stub: emit an import instruction.
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId(node_id),
        SemIR::ImportDecl{.package_id = SemIR::NameId::None}));
    return;
  }

  // Empty declarations and other unhandled nodes - skip.
  if (kind == Parse::NodeKind::EmptyDecl) {
    return;
  }

  // If it's an expression that wasn't caught above, try handling it.
  if (kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
    HandleExpr(context, node_id);
    return;
  }
}

}  // namespace TinySwift::Check
