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
  auto opt_name_node = context.TakePendingOptName();  // for if-let binding

  SemIR::InstId cond_id = SemIR::InstId::None;
  SemIR::InstId opt_payload_id = SemIR::InstId::None;
  SemIR::NameId opt_binding_name_id = SemIR::NameId::None;
  SemIR::TypeId opt_inner_type = SemIR::ErrorInst::TypeId;

  if (cond_node.has_value() &&
      context.node_kind(cond_node) ==
          Parse::NodeKind::OptionalBindingCondition) {
    // OBC has exactly 1 child: the optional value expression.
    // The binding name (IdentifierPattern) is a sibling captured separately.
    SemIR::InstId opt_val_id = SemIR::InstId::None;
    for (auto c : context.children_source_order(cond_node)) {
      auto ck = context.node_kind(c);
      if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        opt_val_id = HandleExpr(context, c);
        break;
      }
    }

    if (opt_val_id.has_value()) {
      auto opt_type = context.insts().Get(opt_val_id).type_id();
      auto bool_type = context.GetBuiltinType("Bool");
      // Extract has-value flag: TupleAccess(opt, 0) == the i1 field.
      cond_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(cond_node),
          SemIR::TupleAccess{.type_id = bool_type,
                             .tuple_id = opt_val_id,
                             .index = SemIR::ElementIndex(0)}));

      // Determine inner type for payload binding in the then-block.
      auto ti = context.types().GetTypeInstId(opt_type);
      if (ti.has_value()) {
        if (auto ot = context.insts().Get(ti).TryAs<SemIR::OptionalType>()) {
          opt_inner_type =
              context.types().GetTypeIdForTypeInstId(ot->inner_type_id);
          opt_payload_id = opt_val_id;
        }
      }
    }

    // Extract binding name from the preceding IdentifierPattern sibling.
    if (opt_name_node.has_value()) {
      auto text = context.token_text(context.node_token(opt_name_node));
      opt_binding_name_id = SemIR::NameId::ForIdentifier(
          context.identifiers().Add(text));
    }
  } else {
    cond_id = cond_node.has_value() ? HandleExpr(context, cond_node)
                                    : SemIR::InstId::None;
  }

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
    // For `if let v = opt { ... }`, inject the unwrapped payload binding at
    // the start of the then-block so that `v` is in scope for the body.
    if (opt_payload_id.has_value() && opt_binding_name_id.has_value()) {
      auto payload_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(then_block_node.has_value() ? then_block_node : node_id),
          SemIR::TupleAccess{.type_id = opt_inner_type,
                             .tuple_id = opt_payload_id,
                             .index = SemIR::ElementIndex(1)}));
      auto ename_id = context.entity_names().Add(
          {.name_id = opt_binding_name_id,
           .parent_scope_id = context.CurrentScopeId()});
      auto binding_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(then_block_node.has_value() ? then_block_node : node_id),
          SemIR::ValueBinding{.type_id = opt_inner_type,
                              .entity_name_id = ename_id,
                              .value_id = payload_id}));
      context.AddNameToScope(opt_binding_name_id, binding_id);
    }
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

  // Build body block: push body_block_id directly so that any nested
  // SwitchInstBlock (from inner if/while) finalizes body_block_id with the
  // correct first-block instructions rather than an orphaned inner block.
  {
    context.PushInstBlockWithId(body_block_id);
    context.AddBodyBlock(body_block_id);
    // Push loop context so break/continue can find their targets.
    context.PushLoopContext(merge_block_id, cond_block_id);
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }
    context.PopLoopContext();
    // Back-edge goes into the tail block (may differ from body_block_id if
    // inner control flow called SwitchInstBlock).
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));
    // Finalize the tail block (whatever is currently on top of the stack).
    context.PopInstBlock();
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

// Handles a for-in statement desugared to a while-loop over an integer range.
//
// `for i in lo...hi { body }` desugars to:
//   var i = lo
//   var _hi = hi
//   bb_entry: ... br bb_cond
//   bb_cond:  i <= _hi ? br bb_body : br bb_merge
//   bb_body:  body; i = i + 1; br bb_cond
//   bb_merge: ...
//
// `for i in lo..<hi { body }` uses `i < _hi` as the condition.
auto HandleForInStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // Extract: pattern node, sequence node, body node.
  Parse::NodeId pattern_node = Parse::NodeId::None;
  Parse::NodeId sequence_node = Parse::NodeId::None;
  Parse::NodeId body_node = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ForInIntroducer) {
      continue;
    }
    if (child_kind == Parse::NodeKind::IdentifierPattern) {
      pattern_node = child;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_node = child;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr) &&
               !sequence_node.has_value()) {
      sequence_node = child;
    }
  }

  // Check that the sequence is a range expression (`lo...hi` or `lo..<hi`).
  if (!sequence_node.has_value() ||
      context.node_kind(sequence_node) != Parse::NodeKind::InfixOperatorExpr) {
    // Unsupported sequence type — fall through with a simple body execution.
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }
    return;
  }

  // Determine operator type: inclusive (`...`) or exclusive (`..<`).
  auto op_text =
      context.token_text(context.node_token(sequence_node));
  bool inclusive = (op_text == "...");

  // Get lower and upper bound nodes from the InfixOperatorExpr children.
  auto range_children = context.children_source_order(sequence_node);
  if (range_children.size() < 2) {
    if (body_node.has_value()) HandleCodeBlock(context, body_node);
    return;
  }
  Parse::NodeId lo_node = range_children[0];
  Parse::NodeId hi_node = range_children[1];

  // Evaluate lower and upper bounds in the current (entry) block.
  auto lo_id = HandleExpr(context, lo_node);
  auto hi_id = HandleExpr(context, hi_node);

  auto int_type = context.GetBuiltinType("Int");
  auto bool_type = context.GetBuiltinType("Bool");

  // Create VarStorage for the loop variable.
  SemIR::NameId loop_var_name_id = SemIR::NameId::None;
  if (pattern_node.has_value()) {
    auto token = context.node_token(pattern_node);
    auto text = context.token_text(token);
    auto ident_id = context.identifiers().Add(text);
    loop_var_name_id = SemIR::NameId::ForIdentifier(ident_id);
  }

  auto var_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(pattern_node.has_value() ? pattern_node : node_id),
      SemIR::VarStorage{.type_id = int_type,
                        .pattern_id = SemIR::AbsoluteInstId(
                            SemIR::InstId::None)}));

  // Initialize loop variable to lower bound.
  if (lo_id.has_value()) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Assign{.lhs_id = var_id, .rhs_id = lo_id}));
  }

  // Register loop variable in the current scope.
  if (loop_var_name_id.has_value()) {
    context.AddNameToScope(loop_var_name_id, var_id);
  }

  // Create block IDs for the loop CFG.
  auto cond_block_id = context.inst_blocks().AddPlaceholder();
  auto body_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Entry block: branch to condition block.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));

  // Build condition block.
  {
    context.PushInstBlock();

    // Load loop variable value.
    auto loop_val_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::NameRef{.type_id = int_type,
                       .name_id = loop_var_name_id,
                       .value_id = var_id}));

    // Emit comparison: loop_var <= hi (inclusive) or loop_var < hi (exclusive).
    SemIR::InstId cond_id = SemIR::InstId::None;
    if (inclusive) {
      cond_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntLessEq{
              .type_id = bool_type, .lhs_id = loop_val_id, .rhs_id = hi_id}));
    } else {
      cond_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntLess{
              .type_id = bool_type, .lhs_id = loop_val_id, .rhs_id = hi_id}));
    }

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

  // Build body block. Use PushInstBlockWithId so that nested control flow
  // (SwitchInstBlock from inner if/while) finalizes body_block_id with the
  // correct entry-block instructions.
  {
    context.PushInstBlockWithId(body_block_id);
    context.AddBodyBlock(body_block_id);
    // Push loop context so break/continue can find their targets.
    context.PushLoopContext(merge_block_id, cond_block_id);

    // Process body statements.
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }

    context.PopLoopContext();

    // Increment loop variable: i = i + 1.
    // These go into the tail block (which may differ from body_block_id).
    auto loop_val_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::NameRef{.type_id = int_type,
                       .name_id = loop_var_name_id,
                       .value_id = var_id}));
    auto one_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntValue{.type_id = int_type,
                        .int_id = context.ints().Add(1)}));
    auto inc_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntAdd{.type_id = int_type,
                      .lhs_id = loop_val_id, .rhs_id = one_id}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Assign{.lhs_id = var_id, .rhs_id = inc_id}));

    // Back-edge: loop back to condition.
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));

    // Finalize the tail block.
    context.PopInstBlock();
  }

  // Switch emission to merge block.
  context.SwitchInstBlock(merge_block_id);
  context.AddBodyBlock(merge_block_id);
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

// Handles a break statement by branching to the enclosing loop's merge block.
auto HandleBreakStatement(Context& context, Parse::NodeId node_id) -> void {
  auto* loop = context.CurrentLoop();
  if (loop) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(loop->break_id)}));
  }
}

// Handles a continue statement by branching to the enclosing loop's condition.
auto HandleContinueStatement(Context& context, Parse::NodeId node_id) -> void {
  auto* loop = context.CurrentLoop();
  if (loop) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(loop->continue_id)}));
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

    // If this is an OptionalBindingCondition (from `if let x = expr`), check
    // if the next sibling is an IfStatement. If so, also capture the preceding
    // IdentifierPattern as the binding name (it's a sibling since OBC has
    // child_count=1 and only takes the value-expression as its child).
    if (child_kind == Parse::NodeKind::OptionalBindingCondition) {
      if (i + 1 < child_vec.size() &&
          context.node_kind(child_vec[i + 1]) == Parse::NodeKind::IfStatement) {
        // Look for the preceding IdentifierPattern sibling (the binding name).
        if (i > 0 &&
            context.node_kind(child_vec[i - 1]) ==
                Parse::NodeKind::IdentifierPattern) {
          context.SetPendingOptName(child_vec[i - 1]);
        }
        context.SetPendingCondition(child);
        continue;
      }
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
  if (kind == Parse::NodeKind::BreakStatement) {
    HandleBreakStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::ContinueStatement) {
    HandleContinueStatement(context, node_id);
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
