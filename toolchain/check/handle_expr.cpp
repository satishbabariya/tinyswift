// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_expr.h"

#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// Helper to get the type of an instruction.
auto GetInstType(Context& context, SemIR::InstId inst_id) -> SemIR::TypeId {
  if (!inst_id.has_value()) {
    return SemIR::ErrorInst::TypeId;
  }
  return context.insts().Get(inst_id).type_id();
}

// Handles an integer literal.
auto HandleIntLiteral(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  // Parse the integer value.
  llvm::APInt value;
  if (text.getAsInteger(0, value)) {
    value = llvm::APInt(64, 0);
  }

  auto int_id = context.ints().Add(static_cast<int64_t>(value.getZExtValue()));
  auto type_id = context.GetBuiltinType("Int");

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::IntValue{.type_id = type_id, .int_id = int_id}));
}

// Handles a float literal.
auto HandleFloatLiteral(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  llvm::APFloat value(llvm::APFloat::IEEEdouble());
  auto status =
      value.convertFromString(text, llvm::APFloat::rmNearestTiesToEven);
  if (!status) {
    value = llvm::APFloat(0.0);
  }

  auto float_id = context.floats().Add(value);
  auto type_id = context.GetBuiltinType("Double");

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::FloatValue{.type_id = type_id, .float_id = float_id}));
}

// Handles a bool literal (true/false).
auto HandleBoolLiteral(Context& context, Parse::NodeId node_id, bool value)
    -> SemIR::InstId {
  auto type_id = context.GetBuiltinType("Bool");
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BoolLiteral{.type_id = type_id,
                         .value = SemIR::BoolValue::From(value)}));
}

// Handles a string literal.
auto HandleStringLiteral(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  // Strip surrounding quotes.
  llvm::StringRef content = text;
  if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
    content = content.drop_front(1).drop_back(1);
  }

  auto string_id = context.string_literal_values().Add(content);
  auto type_id = context.GetBuiltinType("String");

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::StringLiteral{.type_id = type_id, .string_id = string_id}));
}

// Handles a nil literal.
auto HandleNilLiteral(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::OptionalNone{.type_id = SemIR::ErrorInst::TypeId}));
}

// Handles an identifier expression (name reference).
auto HandleIdentifierNameExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  auto ident_id = context.identifiers().Lookup(text);
  if (!ident_id.has_value()) {
    // Name not found. Return error.
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto value_id = context.LookupName(name_id);
  if (!value_id.has_value()) {
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  auto type_id = GetInstType(context, value_id);
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::NameRef{
          .type_id = type_id, .name_id = name_id, .value_id = value_id}));
}

// Handles a parenthesized expression.
auto HandleParenExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      return HandleExpr(context, child);
    }
  }
  return SemIR::InstId::None;
}

// Handles a call expression.
auto HandleCallExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId callee_id = SemIR::InstId::None;
  llvm::SmallVector<SemIR::InstId> arg_ids;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::CallExprStart) {
      // CallExprStart has the callee as its child.
      auto start_children = context.tree_and_subtrees().children(child);
      for (auto start_child : start_children) {
        if (context.node_kind(start_child)
                .category()
                .HasAnyOf(Parse::NodeCategory::Expr)) {
          callee_id = HandleExpr(context, start_child);
          break;
        }
      }
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // Argument expression.
      arg_ids.push_back(HandleExpr(context, child));
    }
  }

  // Create an args block.
  auto args_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      args_block_id, llvm::ArrayRef<SemIR::InstId>(arg_ids));

  // Determine return type (for now, use error type).
  auto type_id = SemIR::ErrorInst::TypeId;
  if (callee_id.has_value()) {
    auto callee_inst = context.insts().Get(callee_id);
    if (auto fn_decl = callee_inst.TryAs<SemIR::FunctionDecl>()) {
      auto& fn = context.functions().Get(fn_decl->function_id);
      if (fn.return_type_inst_id.has_value()) {
        type_id = context.types().GetTypeIdForTypeInstId(
            fn.return_type_inst_id);
      }
    }
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Call{
          .type_id = type_id, .callee_id = callee_id, .args_id = args_block_id}));
}

// Handles a member access expression (base.member).
auto HandleMemberAccessExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId base_id = SemIR::InstId::None;
  llvm::StringRef member_name;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      if (!base_id.has_value()) {
        base_id = HandleExpr(context, child);
      }
    } else if (child_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
               child_kind == Parse::NodeKind::IdentifierNameBeforeParams ||
               child_kind.category().HasAnyOf(
                   Parse::NodeCategory::MemberName)) {
      member_name = context.token_text(context.node_token(child));
    }
  }

  // Try to resolve the member in the base type's scope.
  auto base_type_id = GetInstType(context, base_id);
  auto type_id = SemIR::ErrorInst::TypeId;
  SemIR::ElementIndex element_index(0);

  // For struct/class types, look up the member.
  if (base_type_id.has_value() && base_type_id.is_concrete()) {
    auto type_inst_id = context.types().GetTypeInstId(base_type_id);
    if (type_inst_id.has_value()) {
      auto type_inst = context.insts().Get(type_inst_id);
      if (auto struct_type = type_inst.TryAs<SemIR::StructType>()) {
        // Look up member in scope.
        if (!member_name.empty()) {
          auto ident_id = context.identifiers().Lookup(member_name);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            // Search the struct's scope for the member.
            // For now, return the base's type.
            auto member_inst_id = context.LookupName(name_id);
            if (member_inst_id.has_value()) {
              type_id = GetInstType(context, member_inst_id);
            }
          }
        }
      }
    }
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::FieldAccess{
          .type_id = type_id, .base_id = base_id, .index = element_index}));
}

// Handles an infix operator expression.
auto HandleInfixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId lhs_id = SemIR::InstId::None;
  SemIR::InstId rhs_id = SemIR::InstId::None;
  llvm::StringRef op_text;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      if (!lhs_id.has_value()) {
        lhs_id = HandleExpr(context, child);
      } else {
        rhs_id = HandleExpr(context, child);
      }
    }
  }

  op_text = context.token_text(context.node_token(node_id));

  auto lhs_type = GetInstType(context, lhs_id);
  auto bool_type = context.GetBuiltinType("Bool");

  // Dispatch based on operator.
  if (op_text == "+") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntAdd{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "-") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntSub{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "*") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntMul{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "/") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntDiv{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "%") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntMod{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "==") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntEq{.type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "!=") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntNeq{.type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "<") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntLess{.type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == ">") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntGreater{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "<=") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntLessEq{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == ">=") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntGreaterEq{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "&&") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolAnd{.type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "||") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolOr{.type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }

  // Unknown operator - just return lhs.
  return lhs_id;
}

// Handles a prefix operator expression.
auto HandlePrefixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId operand_id = SemIR::InstId::None;
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      operand_id = HandleExpr(context, child);
      break;
    }
  }

  auto op_text = context.token_text(context.node_token(node_id));
  auto operand_type = GetInstType(context, operand_id);

  if (op_text == "-") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntNegate{.type_id = operand_type, .operand_id = operand_id}));
  }
  if (op_text == "!") {
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolNot{.type_id = operand_type, .operand_id = operand_id}));
  }

  return operand_id;
}

// Handles a postfix operator expression.
auto HandlePostfixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId operand_id = SemIR::InstId::None;
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      operand_id = HandleExpr(context, child);
      break;
    }
  }

  // Postfix ! (force unwrap) and ? (optional chaining) - stubs.
  return operand_id;
}

// Handles an assignment expression.
auto HandleAssignmentExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.tree_and_subtrees().children(node_id);

  SemIR::InstId lhs_id = SemIR::InstId::None;
  SemIR::InstId rhs_id = SemIR::InstId::None;

  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      if (!lhs_id.has_value()) {
        lhs_id = HandleExpr(context, child);
      } else {
        rhs_id = HandleExpr(context, child);
      }
    }
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Assign{.lhs_id = lhs_id, .rhs_id = rhs_id}));
}

}  // namespace

auto HandleExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId {
  if (!node_id.has_value()) {
    return SemIR::InstId::None;
  }

  auto kind = context.node_kind(node_id);

  if (kind == Parse::NodeKind::IntLiteral) {
    return HandleIntLiteral(context, node_id);
  }
  if (kind == Parse::NodeKind::FloatingLiteral) {
    return HandleFloatLiteral(context, node_id);
  }
  if (kind == Parse::NodeKind::BoolLiteralTrue) {
    return HandleBoolLiteral(context, node_id, true);
  }
  if (kind == Parse::NodeKind::BoolLiteralFalse) {
    return HandleBoolLiteral(context, node_id, false);
  }
  if (kind == Parse::NodeKind::StringLiteral) {
    return HandleStringLiteral(context, node_id);
  }
  if (kind == Parse::NodeKind::NilLiteral) {
    return HandleNilLiteral(context, node_id);
  }
  if (kind == Parse::NodeKind::IdentifierNameExpr) {
    return HandleIdentifierNameExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::ParenExpr) {
    return HandleParenExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::CallExpr) {
    return HandleCallExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::MemberAccessExpr) {
    return HandleMemberAccessExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::InfixOperatorExpr) {
    return HandleInfixOperatorExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::PrefixOperatorExpr) {
    return HandlePrefixOperatorExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::PostfixOperatorExpr) {
    return HandlePostfixOperatorExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::AssignmentExpr) {
    return HandleAssignmentExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::SelfExpr) {
    return HandleIdentifierNameExpr(context, node_id);
  }

  // For expression statements, unwrap.
  if (kind == Parse::NodeKind::ExprStatement) {
    auto children = context.tree_and_subtrees().children(node_id);
    for (auto child : children) {
      if (context.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        return HandleExpr(context, child);
      }
    }
  }

  // Unhandled expression kind - return None.
  return SemIR::InstId::None;
}

}  // namespace TinySwift::Check
