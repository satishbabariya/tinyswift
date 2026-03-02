// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_stmt.h"

#include "toolchain/check/handle_decl.h"
#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/lex/token_index.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Handles a return statement.
auto HandleReturnStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  SemIR::InstId expr_id = SemIR::InstId::None;

  for (size_t i = 0; i < children.size(); ++i) {
    auto child = children[i];
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ReturnStatementStart) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // M68: If followed by a CallExpr sibling, this is the orphaned callee
      // from a generic call. Set as pending callee and skip to let the
      // CallExpr handler pick it up.
      if (i + 1 < children.size() &&
          context.node_kind(children[i + 1]) == Parse::NodeKind::CallExpr) {
        context.SetPendingCalleeId(HandleExpr(context, child));
        continue;
      }
      expr_id = HandleExpr(context, child);
    }
  }

  if (expr_id.has_value()) {
    // Coerce return value to Optional<T> when function return type is optional
    // and the expression is not already optional (same pattern as let/var decls).
    auto fn_id = context.CurrentFunctionId();
    if (fn_id.has_value()) {
      auto& fn = context.functions().Get(fn_id);
      if (fn.return_type_inst_id.has_value()) {
        auto ret_inst = context.insts().Get(fn.return_type_inst_id);
        if (ret_inst.Is<SemIR::OptionalType>()) {
          auto expr_inst = context.insts().Get(expr_id);
          bool already_optional =
              expr_inst.Is<SemIR::OptionalNone>() ||
              expr_inst.Is<SemIR::OptionalSome>();
          if (!already_optional) {
            auto expr_ti = context.types().GetTypeInstId(expr_inst.type_id());
            already_optional = expr_ti.has_value() &&
                context.insts().Get(expr_ti).Is<SemIR::OptionalType>();
          }
          if (!already_optional) {
            auto opt_type_id = context.types().GetTypeIdForTypeInstId(
                fn.return_type_inst_id);
            expr_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::OptionalSome{.type_id = opt_type_id,
                                    .value_id = expr_id}));
          }
        }
      }
    }

    // M78: Emit ARC releases for class locals before returning.
    // The return value (if class-typed) is excluded from cleanup.
    context.EmitCleanupReleases(node_id);

    // M51: Emit deferred blocks LIFO before returning.
    // The return expression has already been evaluated (expr_id is set above),
    // so deferred code runs AFTER the return value is captured.
    {
      const auto& deferred = context.GetDeferredBlocks();
      for (int i = static_cast<int>(deferred.size()) - 1; i >= 0; --i) {
        HandleCodeBlock(context, deferred[i]);
      }
    }

    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ReturnExpr{.expr_id = expr_id,
                          .dest_id = SemIR::DestInstId(SemIR::InstId::None)}));
  } else {
    // M51: Emit deferred blocks LIFO before void return.
    {
      const auto& deferred = context.GetDeferredBlocks();
      for (int i = static_cast<int>(deferred.size()) - 1; i >= 0; --i) {
        HandleCodeBlock(context, deferred[i]);
      }
    }
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
  // M50: may be set if condition is OptionalBindingCondition (while let).
  auto opt_name_node = context.TakePendingOptName();

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
  // For `while let v = opt`: evaluate the optional, extract has_value flag.
  SemIR::InstId opt_val_id = SemIR::InstId::None;  // M50: optional value from cond
  SemIR::TypeId opt_inner_type = SemIR::ErrorInst::TypeId;
  SemIR::NameId opt_binding_name_id = SemIR::NameId::None;
  bool is_while_let = false;

  {
    context.PushInstBlock();

    SemIR::InstId cond_id = SemIR::InstId::None;

    if (cond_node.has_value() &&
        context.node_kind(cond_node) ==
            Parse::NodeKind::OptionalBindingCondition) {
      // M50: `while let v = opt { ... }`
      is_while_let = true;
      // OBC has exactly 1 child: the optional value expression.
      for (auto c : context.children_source_order(cond_node)) {
        if (context.node_kind(c).category().HasAnyOf(Parse::NodeCategory::Expr)) {
          opt_val_id = HandleExpr(context, c);
          break;
        }
      }
      if (opt_val_id.has_value()) {
        auto opt_type = context.insts().Get(opt_val_id).type_id();
        auto bool_type = context.GetBuiltinType("Bool");
        // Extract has-value flag: TupleAccess(opt, 0) == the i1 field.
        cond_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(cond_node),
            SemIR::TupleAccess{.type_id = bool_type,
                               .tuple_id = opt_val_id,
                               .index = SemIR::ElementIndex(0)}));
        // Determine inner type for payload binding in the body block.
        auto ti = context.types().GetTypeInstId(opt_type);
        if (ti.has_value()) {
          if (auto ot = context.insts().Get(ti).TryAs<SemIR::OptionalType>()) {
            opt_inner_type =
                context.types().GetTypeIdForTypeInstId(ot->inner_type_id);
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

    // M50: For `while let v = opt`, inject the unwrapped payload binding at
    // the start of the body block so that `v` is in scope for the body.
    if (is_while_let && opt_val_id.has_value() && opt_binding_name_id.has_value()) {
      auto payload_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(body_node.has_value() ? body_node : node_id),
          SemIR::TupleAccess{.type_id = opt_inner_type,
                             .tuple_id = opt_val_id,
                             .index = SemIR::ElementIndex(1)}));
      auto ename_id = context.entity_names().Add(
          {.name_id = opt_binding_name_id,
           .parent_scope_id = context.CurrentScopeId()});
      auto binding_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(body_node.has_value() ? body_node : node_id),
          SemIR::ValueBinding{.type_id = opt_inner_type,
                              .entity_name_id = ename_id,
                              .value_id = payload_id}));
      context.AddNameToScope(opt_binding_name_id, binding_id);
    }

    // Push loop context so break/continue can find their targets.
    context.PushLoopContext(merge_block_id, cond_block_id);
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }
    context.PopLoopContext();
    // Back-edge goes into the tail block (may differ from body_block_id if
    // inner control flow called SwitchInstBlock).
    if (!context.IsCurrentBlockTerminated()) {
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));
    }
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

  // Extract: pattern node, sequence node, where node (optional), body node.
  Parse::NodeId pattern_node = Parse::NodeId::None;
  Parse::NodeId sequence_node = Parse::NodeId::None;
  Parse::NodeId where_node = Parse::NodeId::None;   // M63: where clause
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
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // First Expr is the sequence; second Expr (if any) is the where condition.
      if (!sequence_node.has_value()) {
        sequence_node = child;
      } else if (!where_node.has_value()) {
        where_node = child;  // M63: where clause expression
      }
    }
  }

  // Determine if the sequence is a range or array for-in.
  bool is_range = sequence_node.has_value() &&
      context.node_kind(sequence_node) == Parse::NodeKind::InfixOperatorExpr;

  // Array for-in: sequence is an IdentifierNameExpr (or non-range expression).
  if (!is_range && sequence_node.has_value()) {
    // Evaluate the array expression.
    auto array_ref_id = HandleExpr(context, sequence_node);

    // M99: Generator<T> for-in → while-loop with next() calls.
    // Desugars to: while let <binding> = gen.next() { body }
    if (array_ref_id.has_value()) {
      auto gen_expr_type = context.insts().Get(array_ref_id).type_id();
      auto gen_type_inst_id = context.types().GetTypeInstId(gen_expr_type);
      if (gen_type_inst_id.has_value()) {
        auto gen_type_inst = context.insts().Get(gen_type_inst_id);
        if (gen_type_inst.Is<SemIR::GeneratorType>()) {
          auto gen_ty = gen_type_inst.As<SemIR::GeneratorType>();
          auto elem_type_id =
              context.types().GetTypeIdForTypeInstId(gen_ty.element_type_id);
          auto int_type = context.GetBuiltinType("Int");
          auto bool_type = context.GetBuiltinType("Bool");

          // Get loop variable name from pattern.
          SemIR::NameId loop_var_name_id = SemIR::NameId::None;
          if (pattern_node.has_value()) {
            auto text =
                context.token_text(context.node_token(pattern_node));
            auto ident_id = context.identifiers().Add(text);
            loop_var_name_id = SemIR::NameId::ForIdentifier(ident_id);
          }

          // Build Optional<T> type.
          auto opt_type_inst = context.AddInstInNoBlock(
              SemIR::LocIdAndInst::NoLoc(SemIR::OptionalType{
                  .type_id = SemIR::TypeType::TypeId,
                  .inner_type_id = gen_ty.element_type_id}));
          auto opt_type = SemIR::TypeId::ForTypeConstant(
              SemIR::ConstantId::ForConcreteConstant(opt_type_inst));

          // Create blocks.
          auto cond_block_id = context.inst_blocks().AddPlaceholder();
          auto body_block_id = context.inst_blocks().AddPlaceholder();
          auto merge_block_id = context.inst_blocks().AddPlaceholder();

          // Entry → cond_block.
          context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));

          // Cond block: call next() on generator.
          SemIR::InstId opt_result_id = SemIR::InstId::None;
          {
            context.PushInstBlock();

            // TupleAccess(gen, 0) → frame_ptr
            auto frame_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::TupleAccess{.type_id = int_type,
                                   .tuple_id = array_ref_id,
                                   .index = SemIR::ElementIndex(0)}));
            // TupleAccess(gen, 1) → resume_fn_ptr
            auto resume_fn_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::TupleAccess{.type_id = int_type,
                                   .tuple_id = array_ref_id,
                                   .index = SemIR::ElementIndex(1)}));

            // Call resume_fn(frame_ptr) → Optional<T>
            auto args_block = context.inst_blocks().AddPlaceholder();
            context.inst_blocks().ReplacePlaceholder(
                args_block,
                llvm::ArrayRef<SemIR::InstId>({frame_id}));
            opt_result_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::Call{.type_id = opt_type,
                            .callee_id = resume_fn_id,
                            .args_id = args_block}));

            // Check has_value: TupleAccess(opt_result, 0) → Bool
            auto has_value = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::TupleAccess{.type_id = bool_type,
                                   .tuple_id = opt_result_id,
                                   .index = SemIR::ElementIndex(0)}));

            context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::BranchIf{
                    .target_id = SemIR::LabelId(body_block_id),
                    .cond_id = has_value}));
            context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::Branch{
                    .target_id = SemIR::LabelId(merge_block_id)}));

            auto tmp = context.PopInstBlock();
            auto cond_insts = context.inst_blocks().Get(tmp);
            context.inst_blocks().ReplacePlaceholder(
                cond_block_id,
                llvm::ArrayRef<SemIR::InstId>(cond_insts));
            context.AddBodyBlock(cond_block_id);
          }

          // Body block: extract payload, bind loop var, execute body.
          {
            context.PushInstBlockWithId(body_block_id);
            context.AddBodyBlock(body_block_id);
            context.PushLoopContext(merge_block_id, cond_block_id);

            // Extract payload: TupleAccess(opt_result, 1) → T
            auto payload_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::TupleAccess{
                    .type_id = elem_type_id,
                    .tuple_id = opt_result_id,
                    .index = SemIR::ElementIndex(1)}));

            // Bind loop variable.
            if (loop_var_name_id.has_value()) {
              auto entity_name_id = context.entity_names().Add(
                  {.name_id = loop_var_name_id,
                   .parent_scope_id = context.CurrentScopeId()});
              auto binding_id = context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(pattern_node.has_value() ? pattern_node
                                                        : node_id),
                  SemIR::ValueBinding{
                      .type_id = elem_type_id,
                      .entity_name_id = entity_name_id,
                      .value_id = payload_id}));
              context.AddNameToScope(loop_var_name_id, binding_id);
            }

            // Execute body.
            if (body_node.has_value()) HandleCodeBlock(context, body_node);
            context.PopLoopContext();

            // Branch back to cond.
            if (!context.IsCurrentBlockTerminated()) {
              context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::Branch{
                      .target_id = SemIR::LabelId(cond_block_id)}));
            }
            context.PopInstBlock();
          }

          context.SwitchInstBlock(merge_block_id);
          context.AddBodyBlock(merge_block_id);
          return;
        }
      }
    }

    // Look through NameRef → VarStorage/ValueBinding → ArrayLiteralInit
    // to find the element count and type.
    SemIR::InstId ali_id = SemIR::InstId::None;
    if (array_ref_id.has_value()) {
      auto arr_inst = context.insts().Get(array_ref_id);
      SemIR::InstId real_id = array_ref_id;
      if (auto nr = arr_inst.TryAs<SemIR::NameRef>()) {
        real_id = nr->value_id;
        arr_inst = context.insts().Get(real_id);
      }
      if (arr_inst.Is<SemIR::VarStorage>()) {
        ali_id = context.GetArrayVarInit(real_id);
      } else if (auto vb = arr_inst.TryAs<SemIR::ValueBinding>()) {
        ali_id = vb->value_id;
      }
    }

    if (!ali_id.has_value()) {
      // Fallback: can't determine array info.
      if (body_node.has_value()) HandleCodeBlock(context, body_node);
      return;
    }

    auto ali_inst = context.insts().Get(ali_id);
    auto ali = ali_inst.TryAs<SemIR::ArrayLiteralInit>();
    if (!ali) {
      if (body_node.has_value()) HandleCodeBlock(context, body_node);
      return;
    }

    int64_t count = static_cast<int64_t>(
        context.inst_blocks().Get(ali->elements_id).size());
    auto elem_type_id = ali->type_id;
    auto int_type = context.GetBuiltinType("Int");
    auto bool_type = context.GetBuiltinType("Bool");

    // Create the loop variable name (from pattern_node).
    SemIR::NameId loop_var_name_id = SemIR::NameId::None;
    if (pattern_node.has_value()) {
      auto text = context.token_text(context.node_token(pattern_node));
      auto ident_id = context.identifiers().Add(text);
      loop_var_name_id = SemIR::NameId::ForIdentifier(ident_id);
    }

    // Create a synthetic index VarStorage (e.g., `_for_idx`).
    auto idx_ident_id =
        context.identifiers().Add(context.AllocateString("_for_idx"));
    auto idx_name_id = SemIR::NameId::ForIdentifier(idx_ident_id);

    auto idx_var_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::VarStorage{.type_id = int_type,
                          .pattern_id = SemIR::AbsoluteInstId(
                              SemIR::InstId::None)}));
    auto zero_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntValue{.type_id = int_type, .int_id = context.ints().Add(0)}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Assign{.lhs_id = idx_var_id, .rhs_id = zero_id}));
    context.AddNameToScope(idx_name_id, idx_var_id);

    // Emit count as compile-time constant.
    auto count_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntValue{.type_id = int_type,
                        .int_id = context.ints().Add(count)}));

    // Create block IDs.
    auto cond_block_id = context.inst_blocks().AddPlaceholder();
    auto body_block_id = context.inst_blocks().AddPlaceholder();
    auto merge_block_id = context.inst_blocks().AddPlaceholder();

    // Entry → condition block.
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));

    // Condition block: idx < count.
    {
      context.PushInstBlock();
      auto idx_val_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::NameRef{.type_id = int_type,
                         .name_id = idx_name_id,
                         .value_id = idx_var_id}));
      auto cond_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntLess{.type_id = bool_type,
                         .lhs_id = idx_val_id,
                         .rhs_id = count_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::BranchIf{.target_id = SemIR::LabelId(body_block_id),
                           .cond_id = cond_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
      auto tmp = context.PopInstBlock();
      auto cond_insts = context.inst_blocks().Get(tmp);
      context.inst_blocks().ReplacePlaceholder(
          cond_block_id, llvm::ArrayRef<SemIR::InstId>(cond_insts));
      context.AddBodyBlock(cond_block_id);
    }

    // Body block.
    {
      context.PushInstBlockWithId(body_block_id);
      context.AddBodyBlock(body_block_id);
      context.PushLoopContext(merge_block_id, cond_block_id);

      // Load current index.
      auto idx_val_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::NameRef{.type_id = int_type,
                         .name_id = idx_name_id,
                         .value_id = idx_var_id}));
      // Load element: ArrayAccess{ali_id, idx}.
      auto elem_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::ArrayAccess{.type_id = elem_type_id,
                             .array_id = ali_id,
                             .index_id = idx_val_id}));
      // Bind loop variable.
      if (loop_var_name_id.has_value()) {
        auto entity_name_id = context.entity_names().Add(
            {.name_id = loop_var_name_id,
             .parent_scope_id = context.CurrentScopeId()});
        auto binding_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(pattern_node.has_value() ? pattern_node : node_id),
            SemIR::ValueBinding{.type_id = elem_type_id,
                                .entity_name_id = entity_name_id,
                                .value_id = elem_id}));
        context.AddNameToScope(loop_var_name_id, binding_id);
      }

      // M63: If where clause is present, wrap body execution conditionally.
      if (where_node.has_value()) {
        auto where_exec_id = context.inst_blocks().AddPlaceholder();
        auto inc_block_id = context.inst_blocks().AddPlaceholder();
        auto where_cond_id = HandleExpr(context, where_node);
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::BranchIf{.target_id = SemIR::LabelId(where_exec_id),
                             .cond_id = where_cond_id}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(inc_block_id)}));
        // body_block_id terminated; SwitchInstBlock will pop+finalize it.
        context.SwitchInstBlock(where_exec_id);
        context.AddBodyBlock(where_exec_id);
        if (body_node.has_value()) HandleCodeBlock(context, body_node);
        if (!context.IsCurrentBlockTerminated()) {
          context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::Branch{.target_id = SemIR::LabelId(inc_block_id)}));
        }
        context.PopLoopContext();
        context.SwitchInstBlock(inc_block_id);
        context.AddBodyBlock(inc_block_id);
      } else {
        if (body_node.has_value()) HandleCodeBlock(context, body_node);
        context.PopLoopContext();
      }

      // Increment index (goes into current block: body_block_id or inc_block_id).
      auto idx_reload_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::NameRef{.type_id = int_type,
                         .name_id = idx_name_id,
                         .value_id = idx_var_id}));
      auto one_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntValue{.type_id = int_type,
                          .int_id = context.ints().Add(1)}));
      auto inc_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntAdd{.type_id = int_type,
                        .lhs_id = idx_reload_id,
                        .rhs_id = one_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = idx_var_id, .rhs_id = inc_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));
      context.PopInstBlock();
    }

    context.SwitchInstBlock(merge_block_id);
    context.AddBodyBlock(merge_block_id);
    return;
  }

  // Non-range, non-array: unsupported.
  if (!is_range) {
    if (body_node.has_value()) HandleCodeBlock(context, body_node);
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

    // M63: If where clause present, conditionally execute body.
    if (where_node.has_value()) {
      auto where_exec_id = context.inst_blocks().AddPlaceholder();
      auto inc_block_id = context.inst_blocks().AddPlaceholder();
      auto where_cond_id = HandleExpr(context, where_node);
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::BranchIf{.target_id = SemIR::LabelId(where_exec_id),
                           .cond_id = where_cond_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(inc_block_id)}));
      // SwitchInstBlock finalizes body_block_id and starts where_exec_id.
      context.SwitchInstBlock(where_exec_id);
      context.AddBodyBlock(where_exec_id);
      if (body_node.has_value()) HandleCodeBlock(context, body_node);
      if (!context.IsCurrentBlockTerminated()) {
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(inc_block_id)}));
      }
      context.PopLoopContext();
      context.SwitchInstBlock(inc_block_id);
      context.AddBodyBlock(inc_block_id);
    } else {
      // Process body statements.
      if (body_node.has_value()) {
        HandleCodeBlock(context, body_node);
      }
      context.PopLoopContext();
    }

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

// Handles a switch statement desugared to an if-else comparison chain.
//
// CFG structure:
//   entry: scrutinee + cond1; BranchIf(cond1, body1), Branch(check2)
//   check2: cond2; BranchIf(cond2, body2), Branch(check3 or merge)
//   ...
//   body1: case1 stmts; Branch(merge)
//   body2: case2 stmts; Branch(merge)
//   merge: code after switch
//
// Pattern expressions for integer cases appear as loose Expr children of
// SwitchStatement in the parse tree (from ExprPattern's child_count=0).
// Enum dot-pattern cases (`.red`) use EnumCasePattern inside SwitchCaseLabel.
auto HandleSwitchStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // ---- Phase 1: Collect scrutinee and arms ----
  SemIR::InstId scrutinee_id = SemIR::InstId::None;
  bool found_scrutinee = false;

  struct CaseArm {
    SemIR::InstId pattern_id = SemIR::InstId::None;  // Loose expr pattern (None if default or dot-syntax)
    bool is_default = false;
    bool is_dot_enum = false;               // True if arm uses EnumCasePattern (.red syntax)
    llvm::StringRef dot_case_name;  // Case name for dot-syntax enum patterns
    llvm::StringRef payload_binding_name;  // Binding name for `(let v)` payload
    Parse::NodeId body_node = Parse::NodeId::None;  // SwitchCaseBody parse node
  };
  llvm::SmallVector<CaseArm> arms;
  SemIR::InstId pending_pattern_id = SemIR::InstId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::SwitchIntroducer) {
      continue;
    }

    if (!found_scrutinee) {
      if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        scrutinee_id = HandleExpr(context, child);
        found_scrutinee = true;
      }
      continue;
    }

    // Loose Expr children (from ExprPattern's child_count=0) are pattern values.
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      pending_pattern_id = HandleExpr(context, child);
      continue;
    }

    if (child_kind == Parse::NodeKind::SwitchCaseBody) {
      CaseArm arm;
      arm.pattern_id = pending_pattern_id;
      pending_pattern_id = SemIR::InstId::None;
      arm.is_default = false;
      arm.is_dot_enum = false;
      arm.body_node = child;

      // Inspect label to detect default vs case, and dot-enum patterns.
      for (auto bc : context.children_source_order(child)) {
        auto bck = context.node_kind(bc);
        if (bck == Parse::NodeKind::SwitchDefaultLabel) {
          arm.is_default = true;
        } else if (bck == Parse::NodeKind::SwitchCaseLabel) {
          for (auto lc : context.children_source_order(bc)) {
            if (context.node_kind(lc) == Parse::NodeKind::EnumCasePattern) {
              arm.is_dot_enum = true;
              // EnumCasePattern token is '.'; case name is the next token.
              // EnumCasePattern is always a leaf (child_count=0), so children
              // iteration finds nothing. Instead use token arithmetic:
              //   dot+0='.'  dot+1=case_name  dot+2='('?  dot+3='let'/'var'/name  dot+4=name
              auto dot_tok = context.node_token(lc);
              auto name_tok = Lex::TokenIndex(dot_tok.index + 1);
              arm.dot_case_name = context.token_text(name_tok);
              // Check for payload binding via token arithmetic.
              auto maybe_open = Lex::TokenIndex(dot_tok.index + 2);
              if (context.token_text(maybe_open) == "(") {
                auto maybe_kw = Lex::TokenIndex(dot_tok.index + 3);
                auto kw_text = context.token_text(maybe_kw);
                if (kw_text == "let" || kw_text == "var") {
                  arm.payload_binding_name =
                      context.token_text(Lex::TokenIndex(dot_tok.index + 4));
                } else {
                  arm.payload_binding_name = kw_text;
                }
              }
              break;
            }
          }
        }
      }

      arms.push_back(arm);
    }
  }

  if (!found_scrutinee || arms.empty()) {
    return;
  }

  // ---- Phase 2: Determine scrutinee type ----
  auto int_type = context.GetBuiltinType("Int");
  auto bool_type = context.GetBuiltinType("Bool");

  bool scrutinee_is_enum = false;
  SemIR::TypeId scrutinee_type = SemIR::ErrorInst::TypeId;
  SemIR::NameScopeId enum_name_scope_id = SemIR::NameScopeId::None;

  if (scrutinee_id.has_value()) {
    scrutinee_type = context.insts().Get(scrutinee_id).type_id();
    if (scrutinee_type.has_value() && scrutinee_type.is_concrete()) {
      auto ti = context.types().GetTypeInstId(scrutinee_type);
      if (ti.has_value()) {
        auto type_inst = context.insts().Get(ti);
        if (auto ed = type_inst.TryAs<SemIR::EnumDecl>()) {
          scrutinee_is_enum = true;
          enum_name_scope_id = ed->name_scope_id;
        }
      }
    }
  }

  // ---- Phase 3: Emit discriminant extraction for enum scrutinee ----
  // Bail out early if scrutinee failed to evaluate.
  if (!scrutinee_id.has_value()) {
    return;
  }
  // For enum scrutinee: extract field 0 (i64 tag) once for reuse.
  SemIR::InstId scrutinee_disc_id = SemIR::InstId::None;
  if (scrutinee_is_enum) {
    scrutinee_disc_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::TupleAccess{.type_id = int_type,
                           .tuple_id = scrutinee_id,
                           .index = SemIR::ElementIndex(0)}));
  }

  // ---- Phase 4: Pre-allocate body block IDs ----
  auto merge_block_id = context.inst_blocks().AddPlaceholder();
  llvm::SmallVector<SemIR::InstBlockId> body_block_ids;
  for (size_t i = 0; i < arms.size(); ++i) {
    body_block_ids.push_back(context.inst_blocks().AddPlaceholder());
  }

  // ---- Phase 5: Build comparison chain ----
  // For each arm (except the last):
  //   emit comparison in current block, BranchIf→body, Branch→next_check
  //   SwitchInstBlock(next_check) + AddBodyBlock(next_check)
  // For the last arm:
  //   emit Branch→body (default) or BranchIf+Branch (case), SwitchInstBlock(merge)
  for (size_t i = 0; i < arms.size(); ++i) {
    auto& arm = arms[i];
    auto body_id = body_block_ids[i];

    SemIR::InstBlockId next_check_id = merge_block_id;
    bool has_next = (i + 1 < arms.size());
    if (has_next) {
      next_check_id = context.inst_blocks().AddPlaceholder();
    }

    if (!arm.is_default) {
      // Build comparison expression.
      SemIR::InstId cond_id = SemIR::InstId::None;

      if (scrutinee_is_enum) {
        // Enum scrutinee: compare discriminants.
        SemIR::InstId pattern_disc_id = SemIR::InstId::None;

        if (arm.is_dot_enum && enum_name_scope_id.has_value()) {
          // Dot-syntax pattern (.red or .success(let v)): look up case by name.
          auto ident_id = context.identifiers().Lookup(arm.dot_case_name);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto& scope = context.name_scopes().Get(enum_name_scope_id);
            auto it = scope.names.find(name_id.index);
            if (it != scope.names.end()) {
              auto case_inst = context.insts().Get(it->second);
              if (auto ec = case_inst.TryAs<SemIR::EnumCase>()) {
                pattern_disc_id = context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::IntValue{
                        .type_id = int_type,
                        .int_id = context.ints().Add(ec->discriminant.index)}));
              } else if (auto ecwp =
                             case_inst.TryAs<SemIR::EnumCaseWithPayload>()) {
                pattern_disc_id = context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::IntValue{
                        .type_id = int_type,
                        .int_id = context.ints().Add(
                            ecwp->discriminant.index)}));
              }
            }
          }
        } else if (arm.pattern_id.has_value()) {
          // Qualified pattern (Color.red via MemberAccessExpr → EnumInit):
          // look through EnumInit to get its discriminant.
          auto pat_inst = context.insts().Get(arm.pattern_id);
          if (auto ei = pat_inst.TryAs<SemIR::EnumInit>()) {
            auto case_inst = context.insts().Get(ei->case_id);
            if (auto ec = case_inst.TryAs<SemIR::EnumCase>()) {
              pattern_disc_id = context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::IntValue{
                      .type_id = int_type,
                      .int_id = context.ints().Add(ec->discriminant.index)}));
            }
          }
        }

        if (scrutinee_disc_id.has_value() && pattern_disc_id.has_value()) {
          cond_id = context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::IntEq{.type_id = bool_type,
                           .lhs_id = scrutinee_disc_id,
                           .rhs_id = pattern_disc_id}));
        }
      } else if (arm.pattern_id.has_value()) {
        // Integer scrutinee: direct equality comparison.
        cond_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntEq{.type_id = bool_type,
                         .lhs_id = scrutinee_id,
                         .rhs_id = arm.pattern_id}));
      }

      if (cond_id.has_value()) {
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::BranchIf{.target_id = SemIR::LabelId(body_id),
                             .cond_id = cond_id}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(next_check_id)}));
      } else {
        // Fallback: unconditional branch to body (pattern not analyzable).
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(body_id)}));
      }
    } else {
      // Default arm: unconditional branch to body.
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(body_id)}));
    }

    if (has_next) {
      context.SwitchInstBlock(next_check_id);
      context.AddBodyBlock(next_check_id);
    } else {
      context.SwitchInstBlock(merge_block_id);
      context.AddBodyBlock(merge_block_id);
    }
  }

  // ---- Phase 6: Build body blocks ----
  // Use PushInstBlockWithId to emit instructions directly into body_block_ids[i].
  // This ensures that if nested control flow (e.g., HandleTryExpr) calls
  // SwitchInstBlock, body_block_ids[i] is properly finalized with the correct
  // instructions, and any continuation blocks are also correctly handled.
  for (size_t i = 0; i < arms.size(); ++i) {
    context.PushInstBlockWithId(body_block_ids[i]);
    // Register the body block BEFORE emitting body statements, so that any
    // continuation blocks created during emission (e.g., by HandleTryExpr's
    // SwitchInstBlock) appear AFTER the body block in the function's block
    // list. SILGen processes blocks in order, so the body block (which defines
    // values like call results) must come before continuation blocks that use
    // those values.
    context.AddBodyBlock(body_block_ids[i]);

    // If this arm has an enum payload binding (e.g., `.success(let v)`),
    // inject the binding at the start of the body block.
    auto& arm = arms[i];
    if (arm.is_dot_enum && !arm.payload_binding_name.empty() &&
        enum_name_scope_id.has_value() && scrutinee_id.has_value()) {
      // Look up the case in the enum scope.
      auto payload_ident_id = context.identifiers().Lookup(arm.dot_case_name);
      if (payload_ident_id.has_value()) {
        auto payload_name_id = SemIR::NameId::ForIdentifier(payload_ident_id);
        auto& scope = context.name_scopes().Get(enum_name_scope_id);
        auto case_it = scope.names.find(payload_name_id.index);
        if (case_it != scope.names.end()) {
          auto case_inst = context.insts().Get(case_it->second);
          if (auto ecwp = case_inst.TryAs<SemIR::EnumCaseWithPayload>()) {
            // Determine payload type.
            auto payload_type_id = context.types().GetTypeIdForTypeInstId(
                SemIR::TypeInstId::UnsafeMake(ecwp->payload_type_id));
            // Extract payload from scrutinee: TupleAccess(scrutinee, 1).
            auto payload_val_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::TupleAccess{.type_id = payload_type_id,
                                   .tuple_id = scrutinee_id,
                                   .index = SemIR::ElementIndex(1)}));
            // Create binding for the payload variable.
            auto bind_ident_id =
                context.identifiers().Add(arm.payload_binding_name);
            auto bind_name_id = SemIR::NameId::ForIdentifier(bind_ident_id);
            auto ename_id = context.entity_names().Add(
                {.name_id = bind_name_id,
                 .parent_scope_id = context.CurrentScopeId()});
            auto binding_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::ValueBinding{.type_id = payload_type_id,
                                    .entity_name_id = ename_id,
                                    .value_id = payload_val_id}));
            context.AddNameToScope(bind_name_id, binding_id);
          }
        }
      }
    }

    // Process body statements (children of SwitchCaseBody, skipping the label).
    for (auto bc : context.children_source_order(arms[i].body_node)) {
      auto bck = context.node_kind(bc);
      if (bck == Parse::NodeKind::SwitchCaseLabel ||
          bck == Parse::NodeKind::SwitchDefaultLabel) {
        continue;
      }
      if (bck.category().HasAnyOf(Parse::NodeCategory::Statement |
                                   Parse::NodeCategory::Decl)) {
        HandleStatement(context, bc);
      } else if (bck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        HandleExpr(context, bc);
      }
    }

    // Add branch to merge only if the body didn't already terminate
    // (e.g., via a return statement).
    if (!context.IsCurrentBlockTerminated()) {
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
    }

    // PopInstBlock finalizes whatever is currently on top of the stack.
    // If no SwitchInstBlock was called during body emission, this pops
    // body_block_ids[i] and finalizes it directly.
    // If SwitchInstBlock WAS called (e.g., by HandleTryExpr inside a do-catch),
    // body_block_ids[i] was already finalized by SwitchInstBlock, and this
    // pops the continuation block instead.
    context.PopInstBlock();
  }
}

// Handles a repeat-while statement.
//
// CFG structure (body executes before condition is checked):
//
//   bb_entry:  <code before repeat-while>
//     br bb_body
//
//   bb_body:
//     <body statements>
//     br bb_cond           ← continues to condition check
//
//   bb_cond:
//     %cond = <evaluate condition>
//     cond_br %cond, bb_body, bb_merge   ← loop back-edge or exit
//
//   bb_merge:
//     <code after repeat-while>
//
// NOTE: The condition is a DIRECT CHILD of RepeatWhileStatement (not a sibling
// stored via SetPendingCondition like WhileCondition). It must be extracted
// from the node's own children, not via TakePendingCondition().
auto HandleRepeatWhileStatement(Context& context, Parse::NodeId node_id)
    -> void {
  auto children = context.children_source_order(node_id);

  Parse::NodeId body_node = Parse::NodeId::None;
  Parse::NodeId cond_node = Parse::NodeId::None;

  // Children in source order: CodeBlock (body) then Expr (condition).
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::CodeBlock) {
      body_node = child;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr) &&
               !cond_node.has_value()) {
      cond_node = child;
    }
  }

  // Create the three block IDs.
  auto body_block_id = context.inst_blocks().AddPlaceholder();
  auto cond_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Entry block branches unconditionally to the body (body runs first).
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(body_block_id)}));

  // Build body block. Use PushInstBlockWithId so that nested control flow
  // (SwitchInstBlock from inner if/while) uses body_block_id as the entry.
  {
    context.PushInstBlockWithId(body_block_id);
    context.AddBodyBlock(body_block_id);
    // break → merge_block; continue → cond_block (re-check condition).
    context.PushLoopContext(merge_block_id, cond_block_id);
    if (body_node.has_value()) {
      HandleCodeBlock(context, body_node);
    }
    context.PopLoopContext();
    // After body, fall through to condition check.
    if (!context.IsCurrentBlockTerminated()) {
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(cond_block_id)}));
    }
    context.PopInstBlock();
  }

  // Build condition block: evaluate condition and branch.
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

  // Switch emission to merge block. Code after the repeat-while goes here.
  context.SwitchInstBlock(merge_block_id);
  context.AddBodyBlock(merge_block_id);
}

// Handles a defer statement (M51).
// Instead of executing the body inline, we push the CodeBlock parse node
// onto the deferred_blocks_ stack. Each return statement will emit all
// deferred blocks LIFO before emitting its ReturnExpr/Return.
auto HandleDeferStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::DeferStatementStart) {
      continue;
    }
    if (child_kind == Parse::NodeKind::CodeBlock) {
      context.PushDeferredBlock(child);
      return;
    }
  }
}

// M98: Handles a `yield expr` statement in a generator function.
auto HandleYieldStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  SemIR::InstId value_id = SemIR::InstId::None;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::YieldStatementStart) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      value_id = HandleExpr(context, child);
    }
  }

  if (!value_id.has_value()) {
    return;
  }

  // Determine the yield element type from the value expression.
  auto value_type = context.insts().Get(value_id).type_id();

  // Emit a Yield instruction. The coroutine transform (Pass 3) will
  // process these to build the state machine.
  context.AddInst(SemIR::LocIdAndInst::NoLoc(
      SemIR::Yield{.type_id = value_type, .value_id = value_id}));
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

// M41: `throw expr` — emits ThrowValue (a terminator).
auto HandleThrowStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);
  SemIR::InstId error_id = SemIR::InstId::None;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ThrowStatementStart) continue;
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      error_id = HandleExpr(context, child);
    }
  }
  // ThrowValue is a terminator — it ends the current block.
  if (!error_id.has_value()) {
    // If no expression, emit a placeholder error inst.
    error_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ThrowValue{.error_id = error_id}));
}

// M81: `do { body } catch { handler }` — full error handling.
// Sets up a catch block target that HandleTryExpr will branch to on error.
auto HandleDoStatement(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  Parse::NodeId try_body_node = Parse::NodeId::None;
  llvm::SmallVector<Parse::NodeId, 2> catch_clause_nodes;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::DoIntroducer) continue;
    if (child_kind == Parse::NodeKind::CodeBlock) {
      try_body_node = child;
    } else if (child_kind == Parse::NodeKind::DoCatchClause) {
      catch_clause_nodes.push_back(child);
    }
  }

  // If no catch clauses, just execute the try body (same as M41).
  if (catch_clause_nodes.empty()) {
    if (try_body_node.has_value()) HandleCodeBlock(context, try_body_node);
    return;
  }

  // Create catch and merge blocks.
  auto catch_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Push catch block so HandleTryExpr can branch to it on error.
  context.PushCatchBlock(catch_block_id);

  // Emit the try body.
  if (try_body_node.has_value()) {
    HandleCodeBlock(context, try_body_node);
  }

  context.PopCatchBlock();

  // After the try body, branch unconditionally to merge — but only if the
  // try body didn't already terminate (e.g., with return).
  bool try_body_terminated = context.IsCurrentBlockTerminated();
  if (!try_body_terminated) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
    // Switch emission to merge block (code after do-catch goes here).
    // Only switch if the try body didn't return — otherwise we'd corrupt
    // the block stack when nested inside switch case PushInstBlock.
    context.SwitchInstBlock(merge_block_id);
    context.AddBodyBlock(merge_block_id);
  }

  // Build the catch block.
  {
    context.PushInstBlock();

    // Clear the error slot at the start of the catch block.
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ErrorClear{}));

    // Execute all catch clause bodies.
    for (auto catch_node : catch_clause_nodes) {
      for (auto cc : context.children_source_order(catch_node)) {
        if (context.node_kind(cc) == Parse::NodeKind::CodeBlock) {
          HandleCodeBlock(context, cc);
          break;  // Only one CodeBlock per catch clause.
        }
      }
    }

    auto tmp_id = context.PopInstBlock();
    auto catch_insts = context.inst_blocks().Get(tmp_id);
    context.inst_blocks().ReplacePlaceholder(
        catch_block_id, llvm::ArrayRef<SemIR::InstId>(catch_insts));
    context.AddBodyBlock(catch_block_id);
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
      if (i + 1 < child_vec.size()) {
        auto next_kind = context.node_kind(child_vec[i + 1]);
        if (next_kind == Parse::NodeKind::IfStatement ||
            next_kind == Parse::NodeKind::WhileStatement) {  // M50: while let
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
        // M39: If followed by a cast node, evaluate LHS and store as pending cast.
        if (next_kind == Parse::NodeKind::IsExpr ||
            next_kind == Parse::NodeKind::AsExpr ||
            next_kind == Parse::NodeKind::AsQuestionExpr ||
            next_kind == Parse::NodeKind::AsExclaimExpr) {
          auto lhs_id = HandleExpr(context, child);
          context.SetPendingCastExpr(lhs_id);
          continue;
        }
        // M68: If followed by a CallExpr, this is the orphaned callee from a
        // generic call (GenericArgumentClause shifts the callee outside
        // CallExpr's subtree). Evaluate and store as pending callee.
        if (next_kind == Parse::NodeKind::CallExpr) {
          auto callee_id = HandleExpr(context, child);
          context.SetPendingCalleeId(callee_id);
          continue;
        }
      }
      // Standalone expression statement.
      HandleExpr(context, child);
      continue;
    }

    // M74: AccessModifier is a sibling before declarations in code blocks.
    if (child_kind == Parse::NodeKind::AccessModifier) {
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      if (text == "public") {
        context.SetPendingAccessLevel(SemIR::AccessLevel::Public);
      } else if (text == "private") {
        context.SetPendingAccessLevel(SemIR::AccessLevel::Private);
      } else if (text == "fileprivate") {
        context.SetPendingAccessLevel(SemIR::AccessLevel::FilePrivate);
      } else {
        context.SetPendingAccessLevel(SemIR::AccessLevel::Internal);
      }
      continue;
    }

    // M75/M76: Attribute nodes are siblings before declarations. Skip them
    // here — they will be extracted by HandleFunctionDecl/HandleFunctionDefinition.
    if (child_kind == Parse::NodeKind::Attribute) {
      context.SetPendingAttribute(child);
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
  if (kind == Parse::NodeKind::TypealiasDecl) {
    HandleTypealiasDecl(context, node_id);
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
  if (kind == Parse::NodeKind::RepeatWhileStatement) {
    HandleRepeatWhileStatement(context, node_id);
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

  // M98: yield statement.
  if (kind == Parse::NodeKind::YieldStatement) {
    HandleYieldStatement(context, node_id);
    return;
  }

  // M41: throw/do statements.
  if (kind == Parse::NodeKind::ThrowStatement) {
    HandleThrowStatement(context, node_id);
    return;
  }
  if (kind == Parse::NodeKind::DoStatement) {
    HandleDoStatement(context, node_id);
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
    // Resolve the module name from IdentifierNameNotBeforeParams children.
    auto children = context.children_source_order(node_id);
    SemIR::NameId package_id = SemIR::NameId::None;
    for (auto child : children) {
      if (context.node_kind(child) ==
          Parse::NodeKind::IdentifierNameNotBeforeParams) {
        auto text = context.token_text(context.node_token(child));
        auto ident_id = context.identifiers().Add(text);
        package_id = SemIR::NameId::ForIdentifier(ident_id);
        break;  // First path component is the module name.
      }
    }
    context.AddInst(SemIR::LocIdAndInst::UncheckedLoc(
        SemIR::LocId(node_id),
        SemIR::ImportDecl{.package_id = package_id}));
    return;
  }

  // M107: Conditional compilation (#if/#else/#endif).
  if (kind == Parse::NodeKind::PoundIfDecl) {
    auto children = context.children_source_order(node_id);

    // Separate children into: condition identifier, then-body, else-body.
    // Structure: PoundIfStart, [IdentifierNameExpr(condition)],
    //            [then-body stmts...], [PoundElseDecl], [else-body stmts...],
    //            [PoundEndifDecl]
    llvm::StringRef condition_name;
    bool in_else = false;
    bool condition_met = false;

    for (auto child : children) {
      auto ck = context.node_kind(child);
      if (ck == Parse::NodeKind::PoundIfStart) {
        continue;
      }
      // The condition identifier follows PoundIfStart.
      if (!condition_met && ck == Parse::NodeKind::IdentifierNameExpr) {
        auto token = context.node_token(child);
        condition_name = context.token_text(token);
        // Check if the flag is in the defines set.
        condition_met = context.HasDefine(condition_name);
        continue;
      }
      if (ck == Parse::NodeKind::PoundElseDecl ||
          ck == Parse::NodeKind::PoundElseifDecl) {
        in_else = true;
        continue;
      }
      if (ck == Parse::NodeKind::PoundEndifDecl) {
        break;
      }
      // Process body: execute if in the active branch.
      if ((condition_met && !in_else) || (!condition_met && in_else)) {
        if (ck.category().HasAnyOf(Parse::NodeCategory::Statement |
                                   Parse::NodeCategory::Decl)) {
          HandleStatement(context, child);
        } else if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
          HandleExpr(context, child);
        }
      }
    }
    return;
  }

  // M108: #warning("message") — emit a compile-time warning.
  if (kind == Parse::NodeKind::PoundWarningDecl) {
    auto children = context.children_source_order(node_id);
    for (auto child : children) {
      if (context.node_kind(child) == Parse::NodeKind::StringLiteral) {
        auto token = context.node_token(child);
        auto text = context.token_text(token);
        // Strip surrounding quotes.
        llvm::StringRef content = text;
        if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
          content = content.drop_front(1).drop_back(1);
        }
        // Emit warning diagnostic (don't set has_errors).
        auto warn_token = context.node_token(node_id);
        TINYSWIFT_DIAGNOSTIC(PoundWarningMessage, Warning, "{0}", std::string);
        context.emitter().Emit(warn_token, PoundWarningMessage, content.str());
        break;
      }
    }
    return;
  }

  // M108: #error("message") — emit a compile-time error.
  if (kind == Parse::NodeKind::PoundErrorDecl) {
    auto children = context.children_source_order(node_id);
    for (auto child : children) {
      if (context.node_kind(child) == Parse::NodeKind::StringLiteral) {
        auto token = context.node_token(child);
        auto text = context.token_text(token);
        llvm::StringRef content = text;
        if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
          content = content.drop_front(1).drop_back(1);
        }
        TINYSWIFT_DIAGNOSTIC(PoundErrorMessage, Error, "{0}", std::string);
        context.EmitError(node_id, PoundErrorMessage, content.str());
        break;
      }
    }
    return;
  }

  // M108: #assert(expr) or #assert(expr, "message") — compile-time assertion.
  if (kind == Parse::NodeKind::PoundAssertDecl) {
    auto children = context.children_source_order(node_id);
    SemIR::InstId cond_id = SemIR::InstId::None;
    llvm::StringRef message = "assertion failed";

    for (auto child : children) {
      auto ck = context.node_kind(child);
      if (ck == Parse::NodeKind::PoundAssertStart) continue;
      if (ck == Parse::NodeKind::StringLiteral) {
        auto token = context.node_token(child);
        auto text = context.token_text(token);
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
          message = text.drop_front(1).drop_back(1);
        }
      } else if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        cond_id = HandleExpr(context, child);
      }
    }

    // Evaluate condition: check if it's a boolean literal true.
    if (cond_id.has_value()) {
      auto inst = context.insts().Get(cond_id);
      if (auto bl = inst.TryAs<SemIR::BoolLiteral>()) {
        if (bl->value == SemIR::BoolValue::False) {
          TINYSWIFT_DIAGNOSTIC(PoundAssertFailed, Error, "#assert failed: {0}", std::string);
          context.EmitError(node_id, PoundAssertFailed, message.str());
        }
        // True — no-op.
      }
      // Non-constant expressions are ignored (not a compile-time error).
    }
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
