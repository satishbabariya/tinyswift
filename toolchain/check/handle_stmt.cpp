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
  auto children = context.children_source_order(node_id);

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
  // The condition node was stored by HandleCodeBlock (because IfCondition has
  // child_count=0, leaving the condition expression as a sibling before
  // IfStatement). Evaluate it now in the current block.
  auto cond_node = context.TakePendingCondition();
  SemIR::InstId cond_id =
      cond_node.has_value() ? HandleExpr(context, cond_node)
                            : SemIR::InstId::None;

  auto child_vec = context.children_source_order(node_id);

  Parse::NodeId then_block_node = Parse::NodeId::None;
  Parse::NodeId else_block_node = Parse::NodeId::None;
  bool found_else = false;

  for (auto child : child_vec) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::IfCondition) {
      continue;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr) &&
               !cond_id.has_value()) {
      // Fallback: condition inside IfStatement children.
      cond_id = HandleExpr(context, child);
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

  // Pattern for if-statement lowering:
  //
  //   bb_entry: ... cond_br %cond, bb_then, bb_false
  //   bb_then:  <then body>  [may return or branch to bb_merge]
  //   bb_false: [else body or branch to bb_merge]
  //   bb_merge: <code after the if>
  //
  // In SemIR, cond_br is represented as BranchIf (true target) followed
  // immediately by Branch (false target) in the same block. The SILGen
  // fixup pass combines them into cond_br.
  //
  // After emitting BranchIf + Branch, we call SwitchInstBlock(merge) to
  // finalize the current block and route all subsequent instructions into
  // the merge block.

  // Create the then block and merge block.
  auto then_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Determine the false-path target.
  SemIR::InstBlockId false_block_id =
      else_block_node.has_value()
          ? context.inst_blocks().AddPlaceholder()
          : merge_block_id;

  // Emit BranchIf(true → then) and Branch(false → else/merge) in current block.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(then_block_id),
                       .cond_id = cond_id}));
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(false_block_id)}));

  // Switch emission to merge block. Statements AFTER the if go into merge_block.
  context.SwitchInstBlock(merge_block_id);
  context.AddBodyBlock(merge_block_id);

  // Build the then block.
  {
    context.PushInstBlock();
    if (then_block_node.has_value()) {
      HandleCodeBlock(context, then_block_node);
    }
    auto tmp_id = context.PopInstBlock();
    auto then_insts = context.inst_blocks().Get(tmp_id);
    context.inst_blocks().ReplacePlaceholder(
        then_block_id, llvm::ArrayRef<SemIR::InstId>(then_insts));
    context.AddBodyBlock(then_block_id);
  }

  if (else_block_node.has_value()) {
    // Build the else block.
    context.PushInstBlock();
    auto else_kind = context.node_kind(else_block_node);
    if (else_kind == Parse::NodeKind::CodeBlock) {
      HandleCodeBlock(context, else_block_node);
    } else if (else_kind == Parse::NodeKind::IfStatement) {
      HandleIfStatement(context, else_block_node);
    }
    auto tmp_id = context.PopInstBlock();
    auto else_insts = context.inst_blocks().Get(tmp_id);
    context.inst_blocks().ReplacePlaceholder(
        false_block_id, llvm::ArrayRef<SemIR::InstId>(else_insts));
    context.AddBodyBlock(false_block_id);
  }
}

// Handles a while statement.
//
// Loop CFG structure:
//
//   bb_entry: <code before while>
//     br bb_cond
//
//   bb_cond:
//     %cond = <evaluate condition>
//     cond_br %cond, bb_body, bb_merge
//
//   bb_body:
//     <body statements>
//     br bb_cond           ← back-edge
//
//   bb_merge:
//     <code after while>
//
// We build bb_cond and bb_body as sub-blocks using PushInstBlock/PopInstBlock,
// then call SwitchInstBlock(bb_merge) so subsequent instructions go to bb_merge.
auto HandleWhileStatement(Context& context, Parse::NodeId node_id) -> void {
  // The condition parse node was stored by HandleCodeBlock.
  auto cond_node = context.TakePendingCondition();

  auto child_vec = context.children_source_order(node_id);
  Parse::NodeId body_node = Parse::NodeId::None;

  for (auto child : child_vec) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::WhileCondition) {
      continue;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_node = child;
    }
  }

  // Create the three new block IDs.
  auto cond_block_id = context.inst_blocks().AddPlaceholder();
  auto body_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Current (entry) block branches unconditionally to the condition block.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));

  // Build condition block: evaluate condition, emit cond_br.
  {
    context.PushInstBlock();
    SemIR::InstId cond_id =
        cond_node.has_value() ? HandleExpr(context, cond_node)
                              : SemIR::InstId::None;
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BranchIf{.target_id = SemIR::LabelId(body_block_id),
                         .cond_id = cond_id}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
    auto tmp_id = context.PopInstBlock();
    auto cond_insts = context.inst_blocks().Get(tmp_id);
    context.inst_blocks().ReplacePlaceholder(
        cond_block_id, llvm::ArrayRef<SemIR::InstId>(cond_insts));
    context.AddBodyBlock(cond_block_id);
  }

  // Build body block: process body, then branch back to condition.
  {
    context.PushInstBlock();
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }
    // Back-edge: loop back to condition block.
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));
    auto tmp_id = context.PopInstBlock();
    auto body_insts = context.inst_blocks().Get(tmp_id);
    context.inst_blocks().ReplacePlaceholder(
        body_block_id, llvm::ArrayRef<SemIR::InstId>(body_insts));
    context.AddBodyBlock(body_block_id);
  }

  // Switch emission to merge block. Code after the while goes here.
  context.SwitchInstBlock(merge_block_id);
  context.AddBodyBlock(merge_block_id);
}

// Handles a guard statement.
auto HandleGuardStatement(Context& context, Parse::NodeId node_id) -> void {
  // Evaluate the pending condition node in the current block.
  auto cond_node = context.TakePendingCondition();
  SemIR::InstId cond_id =
      cond_node.has_value() ? HandleExpr(context, cond_node)
                            : SemIR::InstId::None;

  auto child_vec = context.children_source_order(node_id);

  Parse::NodeId else_block_node = Parse::NodeId::None;

  for (auto child : child_vec) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::GuardCondition) {
      continue;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr) &&
               !cond_id.has_value()) {
      // Fallback: condition inside GuardStatement children.
      cond_id = HandleExpr(context, child);
    } else if (child_kind == Parse::NodeKind::GuardStatementElse) {
      continue;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      else_block_node = child;
    }
  }

  // Guard: if condition is false, branch to else block (which must diverge).
  auto else_block_id = context.inst_blocks().AddPlaceholder();

  // BranchIf with negated condition: branch to else when cond is false.
  auto bool_type = context.GetBuiltinType("Bool");
  auto not_cond_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BoolNot{.type_id = bool_type, .operand_id = cond_id}));

  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(else_block_id),
                       .cond_id = not_cond_id}));

  // Build else block contents.
  llvm::SmallVector<SemIR::InstId> else_insts;
  {
    context.PushInstBlock();
    if (else_block_node.has_value()) {
      HandleCodeBlock(context, else_block_node);
    }
    auto tmp_id = context.PopInstBlock();
    else_insts.append(context.inst_blocks().Get(tmp_id).begin(),
                      context.inst_blocks().Get(tmp_id).end());
  }
  context.inst_blocks().ReplacePlaceholder(
      else_block_id, llvm::ArrayRef<SemIR::InstId>(else_insts));
  context.AddBodyBlock(else_block_id);
}

// Handles a for-in statement (basic desugaring to while loop).
auto HandleForInStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

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
  auto children = context.children_source_order(node_id);

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
  auto children = context.children_source_order(node_id);

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
  auto child_vec = context.children_source_order(node_id);

  // Process children in source order.
  // The parser places condition expressions as siblings BEFORE IfStatement/
  // WhileStatement/GuardStatement (because IfCondition has child_count=0).
  // We detect this pattern and pass the condition to the statement handler.
  for (size_t i = 0; i < child_vec.size(); ++i) {
    auto child = child_vec[i];
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::CodeBlockStart) {
      continue;
    }

    // If this is an expression node, check if the next sibling is an
    // if/while/guard statement. If so, store the parse NodeId as a pending
    // condition for the statement handler to evaluate. The while handler
    // needs to evaluate it inside a dedicated condition block for the
    // loop back-edge; the if/guard handlers evaluate it inline.
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      if (i + 1 < child_vec.size()) {
        auto next_kind = context.node_kind(child_vec[i + 1]);
        if (next_kind == Parse::NodeKind::IfStatement ||
            next_kind == Parse::NodeKind::WhileStatement ||
            next_kind == Parse::NodeKind::GuardStatement) {
          context.SetPendingCondition(child);
          continue;
        }
      }
      // Standalone expression statement.
      HandleExpr(context, child);
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
    auto children = context.children_source_order(node_id);
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
