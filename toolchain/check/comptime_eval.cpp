// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/comptime_eval.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/check/context.h"
#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

// Comptime-specific diagnostics.
// NOLINTBEGIN(readability-identifier-naming)
TINYSWIFT_DIAGNOSTIC(ComptimeNonComptimeCall, Error,
                     "cannot call non-comptime function '{0}' in "
                     "compile-time context",
                     std::string);
TINYSWIFT_DIAGNOSTIC(ComptimeIterationLimit, Error,
                     "compile-time evaluation exceeded iteration limit "
                     "({0})",
                     int);
TINYSWIFT_DIAGNOSTIC(ComptimeDivisionByZero, Error,
                     "division by zero in compile-time evaluation");
TINYSWIFT_DIAGNOSTIC(ComptimeUnsupportedOperation, Error,
                     "unsupported operation in compile-time context: {0}",
                     std::string);
// NOLINTEND(readability-identifier-naming)

ComptimeEvaluator::ComptimeEvaluator(Context& context) : context_(context) {}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvaluateComptimeExpr(Parse::NodeId node_id)
    -> SemIR::InstId {
  // The node_id is the ComptimeExpr node. Its single child is the expression.
  auto children = context_.children_source_order(node_id);
  Parse::NodeId expr_node = Parse::NodeId::None;
  for (auto child : children) {
    if (context_.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      expr_node = child;
      break;
    }
  }
  if (!expr_node.has_value()) {
    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       std::string("empty comptime expression"));
    return SemIR::InstId::None;
  }

  // Reset interpreter state.
  iteration_count_ = 0;
  has_returned_ = false;
  return_value_ = std::nullopt;

  auto result = EvalExpr(expr_node);
  if (!result.has_value()) {
    // Error already emitted by EvalExpr.
    return SemIR::InstId::None;
  }

  return ValueToSemIR(node_id, *result);
}

// ---------------------------------------------------------------------------
// Variable environment
// ---------------------------------------------------------------------------

void ComptimeEvaluator::PushScope() {
  env_stack_.emplace_back();
}

void ComptimeEvaluator::PopScope() {
  if (!env_stack_.empty()) {
    env_stack_.pop_back();
  }
}

void ComptimeEvaluator::SetVar(llvm::StringRef name, ComptimeValue val) {
  if (env_stack_.empty()) {
    env_stack_.emplace_back();
  }
  env_stack_.back()[name] = std::move(val);
}

auto ComptimeEvaluator::GetVar(llvm::StringRef name)
    -> std::optional<ComptimeValue> {
  // Search from innermost scope outward.
  for (auto it = env_stack_.rbegin(); it != env_stack_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return std::nullopt;
}

void ComptimeEvaluator::SetVarMutable(llvm::StringRef name,
                                       ComptimeValue val) {
  // Search from innermost scope outward; update in place if found.
  for (auto it = env_stack_.rbegin(); it != env_stack_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      found->second = std::move(val);
      return;
    }
  }
  // Not found — add to current scope.
  SetVar(name, std::move(val));
}

auto ComptimeEvaluator::CheckIterationLimit(Parse::NodeId loc) -> bool {
  if (++iteration_count_ > kMaxIterations) {
    context_.EmitError(loc, ComptimeIterationLimit, kMaxIterations);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// M113: Comptime function registration
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::RegisterComptimeFunction(SemIR::FunctionId fn_id,
                                                  Parse::NodeId def_node_id)
    -> void {
  comptime_function_defs_.insert_or_assign(fn_id.index, def_node_id);
}

auto ComptimeEvaluator::IsComptimeFunction(SemIR::FunctionId fn_id) const
    -> bool {
  return comptime_function_defs_.count(fn_id.index) > 0;
}

// ---------------------------------------------------------------------------
// EvalExpr — tree-walking expression evaluator
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalExpr(Parse::NodeId node_id)
    -> std::optional<ComptimeValue> {
  if (!node_id.has_value()) return std::nullopt;
  auto kind = context_.node_kind(node_id);

  // --- Literals ---

  if (kind == Parse::NodeKind::IntLiteral) {
    auto token = context_.node_token(node_id);
    auto text = context_.token_text(token);
    llvm::APInt value;
    if (text.getAsInteger(0, value)) {
      value = llvm::APInt(64, 0);
    }
    return ComptimeValue::MakeInt(
        static_cast<int64_t>(value.getZExtValue()));
  }

  if (kind == Parse::NodeKind::FloatingLiteral) {
    auto token = context_.node_token(node_id);
    auto text = context_.token_text(token);
    double d = 0.0;
    text.getAsDouble(d);
    return ComptimeValue::MakeFloat(d);
  }

  if (kind == Parse::NodeKind::BoolLiteralTrue) {
    return ComptimeValue::MakeBool(true);
  }
  if (kind == Parse::NodeKind::BoolLiteralFalse) {
    return ComptimeValue::MakeBool(false);
  }

  if (kind == Parse::NodeKind::StringLiteral) {
    auto token = context_.node_token(node_id);
    auto text = context_.token_text(token);
    // Strip surrounding quotes.
    llvm::StringRef content = text;
    if (content.size() >= 2 && content.front() == '"' &&
        content.back() == '"') {
      content = content.drop_front(1).drop_back(1);
    }
    return ComptimeValue::MakeString(content);
  }

  // --- Identifiers (variable lookup) ---

  if (kind == Parse::NodeKind::IdentifierNameExpr) {
    auto token = context_.node_token(node_id);
    auto name = context_.token_text(token);
    auto val = GetVar(name);
    if (!val.has_value()) {
      context_.EmitError(node_id, ComptimeUnsupportedOperation,
                         "undefined variable '" + name.str() + "'");
      return std::nullopt;
    }
    return val;
  }

  // --- Parenthesized expression ---

  if (kind == Parse::NodeKind::ParenExpr) {
    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        return EvalExpr(child);
      }
    }
    return std::nullopt;
  }

  // --- Prefix operator ---

  if (kind == Parse::NodeKind::PrefixOperatorExpr) {
    auto children = context_.children_source_order(node_id);
    auto token = context_.node_token(node_id);
    auto op_text = context_.token_text(token);

    Parse::NodeId operand_node = Parse::NodeId::None;
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        operand_node = child;
        break;
      }
    }
    if (!operand_node.has_value()) return std::nullopt;

    auto operand = EvalExpr(operand_node);
    if (!operand.has_value()) return std::nullopt;

    if (op_text == "-") {
      if (operand->kind == ComptimeValue::Int)
        return ComptimeValue::MakeInt(-operand->int_val);
      if (operand->kind == ComptimeValue::Float)
        return ComptimeValue::MakeFloat(-operand->float_val);
    }
    if (op_text == "!") {
      return ComptimeValue::MakeBool(!operand->ToBool());
    }
    if (op_text == "~") {
      if (operand->kind == ComptimeValue::Int)
        return ComptimeValue::MakeInt(~operand->int_val);
    }
    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       "prefix operator '" + op_text.str() + "'");
    return std::nullopt;
  }

  // --- Infix operator ---

  if (kind == Parse::NodeKind::InfixOperatorExpr) {
    auto children = context_.children_source_order(node_id);
    auto token = context_.node_token(node_id);
    auto op_text = context_.token_text(token);

    // Children: [lhs, rhs] (the operator token is the node's own token).
    if (children.size() < 2) return std::nullopt;

    // Find lhs and rhs expression children.
    Parse::NodeId lhs_node = Parse::NodeId::None;
    Parse::NodeId rhs_node = Parse::NodeId::None;
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        if (!lhs_node.has_value()) {
          lhs_node = child;
        } else {
          rhs_node = child;
        }
      }
    }
    if (!lhs_node.has_value() || !rhs_node.has_value()) return std::nullopt;

    auto lhs = EvalExpr(lhs_node);
    auto rhs = EvalExpr(rhs_node);
    if (!lhs.has_value() || !rhs.has_value()) return std::nullopt;

    // String concatenation.
    if (lhs->kind == ComptimeValue::String &&
        rhs->kind == ComptimeValue::String && op_text == "+") {
      return ComptimeValue::MakeString(
          llvm::StringRef(lhs->string_val + rhs->string_val));
    }
    if (lhs->kind == ComptimeValue::String &&
        rhs->kind == ComptimeValue::String) {
      if (op_text == "==")
        return ComptimeValue::MakeBool(lhs->string_val == rhs->string_val);
      if (op_text == "!=")
        return ComptimeValue::MakeBool(lhs->string_val != rhs->string_val);
    }

    // Arithmetic / comparison on numeric types.
    if (lhs->IsNumeric() && rhs->IsNumeric()) {
      bool use_float =
          (lhs->kind == ComptimeValue::Float ||
           rhs->kind == ComptimeValue::Float);

      if (use_float) {
        double a = lhs->ToDouble(), b = rhs->ToDouble();
        if (op_text == "+") return ComptimeValue::MakeFloat(a + b);
        if (op_text == "-") return ComptimeValue::MakeFloat(a - b);
        if (op_text == "*") return ComptimeValue::MakeFloat(a * b);
        if (op_text == "/") {
          if (b == 0.0) {
            context_.EmitError(node_id, ComptimeDivisionByZero);
            return std::nullopt;
          }
          return ComptimeValue::MakeFloat(a / b);
        }
        if (op_text == "==") return ComptimeValue::MakeBool(a == b);
        if (op_text == "!=") return ComptimeValue::MakeBool(a != b);
        if (op_text == "<") return ComptimeValue::MakeBool(a < b);
        if (op_text == ">") return ComptimeValue::MakeBool(a > b);
        if (op_text == "<=") return ComptimeValue::MakeBool(a <= b);
        if (op_text == ">=") return ComptimeValue::MakeBool(a >= b);
      } else {
        int64_t a = lhs->ToInt64(), b = rhs->ToInt64();
        if (op_text == "+") return ComptimeValue::MakeInt(a + b);
        if (op_text == "-") return ComptimeValue::MakeInt(a - b);
        if (op_text == "*") return ComptimeValue::MakeInt(a * b);
        if (op_text == "/") {
          if (b == 0) {
            context_.EmitError(node_id, ComptimeDivisionByZero);
            return std::nullopt;
          }
          return ComptimeValue::MakeInt(a / b);
        }
        if (op_text == "%") {
          if (b == 0) {
            context_.EmitError(node_id, ComptimeDivisionByZero);
            return std::nullopt;
          }
          return ComptimeValue::MakeInt(a % b);
        }
        if (op_text == "==") return ComptimeValue::MakeBool(a == b);
        if (op_text == "!=") return ComptimeValue::MakeBool(a != b);
        if (op_text == "<") return ComptimeValue::MakeBool(a < b);
        if (op_text == ">") return ComptimeValue::MakeBool(a > b);
        if (op_text == "<=") return ComptimeValue::MakeBool(a <= b);
        if (op_text == ">=") return ComptimeValue::MakeBool(a >= b);
        // Bitwise operators.
        if (op_text == "&") return ComptimeValue::MakeInt(a & b);
        if (op_text == "|") return ComptimeValue::MakeInt(a | b);
        if (op_text == "^") return ComptimeValue::MakeInt(a ^ b);
        if (op_text == "<<") return ComptimeValue::MakeInt(a << b);
        if (op_text == ">>") return ComptimeValue::MakeInt(a >> b);
      }
    }

    // Logical operators.
    if (op_text == "&&") {
      return ComptimeValue::MakeBool(lhs->ToBool() && rhs->ToBool());
    }
    if (op_text == "||") {
      return ComptimeValue::MakeBool(lhs->ToBool() || rhs->ToBool());
    }

    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       "binary operator '" + op_text.str() + "'");
    return std::nullopt;
  }

  // --- Ternary expression ---

  if (kind == Parse::NodeKind::TernaryExpr) {
    auto children = context_.children_source_order(node_id);
    // Children: [condition, then_expr, else_expr]
    llvm::SmallVector<Parse::NodeId> expr_children;
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        expr_children.push_back(child);
      }
    }
    if (expr_children.size() < 3) return std::nullopt;
    auto cond = EvalExpr(expr_children[0]);
    if (!cond.has_value()) return std::nullopt;
    return cond->ToBool() ? EvalExpr(expr_children[1])
                          : EvalExpr(expr_children[2]);
  }

  // --- Nested comptime (transparent) ---

  if (kind == Parse::NodeKind::ComptimeExpr) {
    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        return EvalExpr(child);
      }
    }
    return std::nullopt;
  }

  // --- M113: Function calls ---

  if (kind == Parse::NodeKind::CallExpr) {
    return EvalCallExpr(node_id);
  }

  // --- M114: Array literal ---

  if (kind == Parse::NodeKind::ArrayExpr) {
    auto children = context_.children_source_order(node_id);
    std::vector<ComptimeValue> elems;
    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::ArrayExprStart ||
          ck == Parse::NodeKind::PatternListComma) {
        continue;
      }
      if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        auto val = EvalExpr(child);
        if (!val.has_value()) return std::nullopt;
        elems.push_back(std::move(*val));
      }
    }
    return ComptimeValue::MakeArray(std::move(elems));
  }

  // --- M114: Member access (array.count, struct.field) ---

  if (kind == Parse::NodeKind::MemberAccessExpr) {
    return EvalMemberAccessExpr(node_id);
  }

  // --- M114: Subscript access (array[index]) ---

  if (kind == Parse::NodeKind::SubscriptExpr) {
    return EvalSubscriptExpr(node_id);
  }

  context_.EmitError(node_id, ComptimeUnsupportedOperation,
                     "unsupported expression in comptime context");
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// M113: EvalCallExpr
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalCallExpr(Parse::NodeId node_id)
    -> std::optional<ComptimeValue> {
  auto children = context_.children_source_order(node_id);

  // Extract callee name, optional object name (for method calls), and arguments.
  std::string callee_name;
  std::string object_name;   // Set for method calls like obj.method(...)
  std::string method_name;
  llvm::SmallVector<ComptimeValue> args;

  for (auto child : children) {
    auto ck = context_.node_kind(child);
    if (ck == Parse::NodeKind::PatternListComma) continue;

    // CallExprStart contains the callee expression.
    if (ck == Parse::NodeKind::CallExprStart) {
      auto start_children = context_.children_source_order(child);
      for (auto sc : start_children) {
        auto sk = context_.node_kind(sc);
        if (sk == Parse::NodeKind::IdentifierNameExpr) {
          auto token = context_.node_token(sc);
          callee_name = context_.token_text(token).str();
        }
        // Method call: obj.method(...)
        if (sk == Parse::NodeKind::MemberAccessExpr) {
          auto ma_children = context_.children_source_order(sc);
          for (auto mc : ma_children) {
            auto mk = context_.node_kind(mc);
            if (mk == Parse::NodeKind::IdentifierNameExpr) {
              auto token = context_.node_token(mc);
              auto text = context_.token_text(token).str();
              if (object_name.empty()) {
                object_name = text;
              } else {
                method_name = text;
              }
            }
          }
        }
      }
      continue;
    }

    if (ck == Parse::NodeKind::IdentifierNameExpr) {
      callee_name = context_.token_text(context_.node_token(child)).str();
      continue;
    }

    if (ck == Parse::NodeKind::CallArgument) {
      // CallArgument has children: optional label, expression.
      auto arg_children = context_.children_source_order(child);
      for (auto ac : arg_children) {
        auto ak = context_.node_kind(ac);
        if (ak == Parse::NodeKind::ArgumentLabel) continue;
        if (ak.category().HasAnyOf(Parse::NodeCategory::Expr)) {
          auto val = EvalExpr(ac);
          if (!val.has_value()) return std::nullopt;
          args.push_back(std::move(*val));
        }
      }
      continue;
    }

    // Bare expression argument (without CallArgument wrapper).
    if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      auto val = EvalExpr(child);
      if (!val.has_value()) return std::nullopt;
      args.push_back(std::move(*val));
    }
  }

  // M114: Handle method calls on arrays/strings.
  if (!object_name.empty() && !method_name.empty()) {
    auto obj = GetVar(object_name);
    if (!obj.has_value()) {
      context_.EmitError(node_id, ComptimeUnsupportedOperation,
                         "undefined variable '" + object_name + "'");
      return std::nullopt;
    }
    if (obj->kind == ComptimeValue::Array && method_name == "append") {
      if (!args.empty()) {
        obj->array_vals.push_back(args[0]);
        SetVarMutable(object_name, std::move(*obj));
      }
      return ComptimeValue::MakeNone();
    }
    if (obj->kind == ComptimeValue::Array && method_name == "count") {
      return ComptimeValue::MakeInt(
          static_cast<int64_t>(obj->array_vals.size()));
    }
    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       "unsupported method '" + method_name + "' on '" +
                           object_name + "'");
    return std::nullopt;
  }

  if (callee_name.empty()) {
    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       "cannot resolve callee in comptime context");
    return std::nullopt;
  }

  // Look up the function by name in the check context.
  auto ident_id = context_.identifiers().Add(callee_name);
  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto fn_inst_id = context_.LookupName(name_id);
  if (!fn_inst_id.has_value()) {
    context_.EmitError(node_id, ComptimeNonComptimeCall,
                       std::string(callee_name));
    return std::nullopt;
  }

  auto fn_inst = context_.insts().Get(fn_inst_id);
  auto fn_decl = fn_inst.TryAs<SemIR::FunctionDecl>();
  if (!fn_decl) {
    // M114: Could be a struct type constructor.
    if (fn_inst.TryAs<SemIR::StructType>()) {
      return EvalStructInit(node_id, callee_name, args);
    }
    context_.EmitError(node_id, ComptimeNonComptimeCall,
                       std::string(callee_name));
    return std::nullopt;
  }

  auto fn_id = fn_decl->function_id;
  if (!IsComptimeFunction(fn_id)) {
    context_.EmitError(node_id, ComptimeNonComptimeCall,
                       std::string(callee_name));
    return std::nullopt;
  }

  return ExecuteFunction(fn_id, args);
}

// ---------------------------------------------------------------------------
// M114: Struct initialization in comptime
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalStructInit(
    Parse::NodeId /*node_id*/, llvm::StringRef type_name,
    llvm::ArrayRef<ComptimeValue> args) -> std::optional<ComptimeValue> {
  // Look up struct definition to get field names.
  auto ident_id = context_.identifiers().Add(type_name);
  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto type_inst_id = context_.LookupName(name_id);
  if (!type_inst_id.has_value()) return std::nullopt;

  auto inst = context_.insts().Get(type_inst_id);
  auto struct_type = inst.TryAs<SemIR::StructType>();
  if (!struct_type) return std::nullopt;

  // Get field names from the struct's name scope.
  auto& scope = context_.name_scopes().Get(struct_type->name_scope_id);
  std::vector<std::pair<std::string, ComptimeValue>> fields;
  size_t arg_idx = 0;

  for (auto& [name_idx, entry] : scope.names) {
    auto field_name_id = SemIR::NameId(name_idx);
    auto ident_opt = field_name_id.AsIdentifierId();
    if (!ident_opt.has_value()) continue;
    auto field_name = context_.identifiers().Get(ident_opt);
    // Skip methods (FunctionDecl).
    auto entry_inst = context_.insts().Get(entry);
    if (entry_inst.Is<SemIR::FunctionDecl>()) continue;

    if (arg_idx < args.size()) {
      fields.push_back({field_name.str(), args[arg_idx]});
      ++arg_idx;
    } else {
      fields.push_back({field_name.str(), ComptimeValue::MakeNone()});
    }
  }

  return ComptimeValue::MakeStruct(type_name, std::move(fields));
}

// ---------------------------------------------------------------------------
// M114: Member access evaluation
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalMemberAccessExpr(Parse::NodeId node_id)
    -> std::optional<ComptimeValue> {
  auto children = context_.children_source_order(node_id);
  // MemberAccessExpr children: [base_expr, member_name_token]
  Parse::NodeId base_node = Parse::NodeId::None;
  llvm::StringRef member_name;

  for (auto child : children) {
    auto ck = context_.node_kind(child);
    if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      base_node = child;
    } else if (ck == Parse::NodeKind::IdentifierNameNotBeforeParams ||
               ck == Parse::NodeKind::IdentifierNameBeforeParams) {
      auto token = context_.node_token(child);
      member_name = context_.token_text(token);
    }
  }

  // If no explicit base expr child, look for the IdentifierNameExpr
  // as first child (base) and the member name token.
  if (!base_node.has_value() && children.size() >= 1) {
    // The token of the MemberAccessExpr itself carries the member name.
    auto token = context_.node_token(node_id);
    member_name = context_.token_text(token);
    // Base is the first child.
    for (auto child : children) {
      if (context_.node_kind(child).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        base_node = child;
        break;
      }
    }
  }

  if (!base_node.has_value()) return std::nullopt;

  auto base = EvalExpr(base_node);
  if (!base.has_value()) return std::nullopt;

  // Array .count
  if (base->kind == ComptimeValue::Array && member_name == "count") {
    return ComptimeValue::MakeInt(
        static_cast<int64_t>(base->array_vals.size()));
  }

  // String .count / .isEmpty
  if (base->kind == ComptimeValue::String) {
    if (member_name == "count")
      return ComptimeValue::MakeInt(
          static_cast<int64_t>(base->string_val.size()));
    if (member_name == "isEmpty")
      return ComptimeValue::MakeBool(base->string_val.empty());
  }

  // Struct field access.
  if (base->kind == ComptimeValue::Struct) {
    for (auto& [fname, fval] : base->struct_fields) {
      if (fname == member_name) {
        return fval;
      }
    }
    context_.EmitError(node_id, ComptimeUnsupportedOperation,
                       "no field '" + member_name.str() + "' in struct");
    return std::nullopt;
  }

  context_.EmitError(node_id, ComptimeUnsupportedOperation,
                     "member access '." + member_name.str() + "'");
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// M114: Subscript access evaluation
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalSubscriptExpr(Parse::NodeId node_id)
    -> std::optional<ComptimeValue> {
  auto children = context_.children_source_order(node_id);
  Parse::NodeId base_node = Parse::NodeId::None;
  Parse::NodeId index_node = Parse::NodeId::None;

  for (auto child : children) {
    auto ck = context_.node_kind(child);
    // Base expression is inside SubscriptExprStart.
    if (ck == Parse::NodeKind::SubscriptExprStart) {
      auto start_children = context_.children_source_order(child);
      for (auto sc : start_children) {
        if (context_.node_kind(sc).category().HasAnyOf(
                Parse::NodeCategory::Expr)) {
          base_node = sc;
        }
      }
      continue;
    }
    if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      index_node = child;
    }
  }

  if (!base_node.has_value() || !index_node.has_value()) return std::nullopt;

  auto base = EvalExpr(base_node);
  auto index = EvalExpr(index_node);
  if (!base.has_value() || !index.has_value()) return std::nullopt;

  if (base->kind == ComptimeValue::Array &&
      index->kind == ComptimeValue::Int) {
    auto idx = index->int_val;
    if (idx < 0 || idx >= static_cast<int64_t>(base->array_vals.size())) {
      context_.EmitError(node_id, ComptimeUnsupportedOperation,
                         "array index out of bounds");
      return std::nullopt;
    }
    return base->array_vals[static_cast<size_t>(idx)];
  }

  context_.EmitError(node_id, ComptimeUnsupportedOperation,
                     "subscript access on non-array type");
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// M113: ExecuteFunction — run a comptime function body
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::ExecuteFunction(SemIR::FunctionId fn_id,
                                         llvm::ArrayRef<ComptimeValue> args)
    -> std::optional<ComptimeValue> {
  auto it = comptime_function_defs_.find(fn_id.index);
  if (it == comptime_function_defs_.end()) {
    return std::nullopt;
  }

  auto def_node_id = it->second;

  // Find the CodeBlock (body) in the FunctionDefinition.
  auto children = context_.children_source_order(def_node_id);
  Parse::NodeId body_node = Parse::NodeId::None;
  Parse::NodeId sig_node = Parse::NodeId::None;
  for (auto child : children) {
    auto ck = context_.node_kind(child);
    if (ck == Parse::NodeKind::CodeBlock) {
      body_node = child;
    } else if (ck == Parse::NodeKind::FunctionDefinitionStart) {
      sig_node = child;
    }
  }
  if (!body_node.has_value()) return std::nullopt;

  // Extract parameter names from the signature.
  // Parse tree: FunctionDefinitionStart → ExplicitParamList → FunctionParam
  // FunctionParam children: [ExternalParamName], IdentifierPattern, TypeAnnotation
  llvm::SmallVector<std::string> param_names;
  if (sig_node.has_value()) {
    auto sig_children = context_.children_source_order(sig_node);
    for (auto sc : sig_children) {
      if (context_.node_kind(sc) == Parse::NodeKind::ExplicitParamList) {
        auto param_list_children = context_.children_source_order(sc);
        for (auto plc : param_list_children) {
          if (context_.node_kind(plc) == Parse::NodeKind::FunctionParam) {
            auto param_children = context_.children_source_order(plc);
            for (auto pc : param_children) {
              auto pk = context_.node_kind(pc);
              if (pk == Parse::NodeKind::IdentifierPattern) {
                auto token = context_.node_token(pc);
                param_names.push_back(context_.token_text(token).str());
              }
            }
          }
        }
      }
    }
  }

  // Push scope and bind parameters.
  PushScope();
  for (size_t i = 0; i < param_names.size() && i < args.size(); ++i) {
    SetVar(param_names[i], args[i]);
  }

  // Save/restore return state.
  auto saved_return = return_value_;
  auto saved_has_returned = has_returned_;
  return_value_ = std::nullopt;
  has_returned_ = false;

  // Execute body.
  EvalCodeBlockBody(body_node);

  auto result = return_value_;
  PopScope();

  // Restore outer return state.
  return_value_ = saved_return;
  has_returned_ = saved_has_returned;

  return result;
}

// ---------------------------------------------------------------------------
// EvalCodeBlockBody — iterate children with condition-as-sibling handling
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalCodeBlockBody(Parse::NodeId code_block_node)
    -> bool {
  auto children = context_.children_source_order(code_block_node);
  llvm::SmallVector<Parse::NodeId> child_vec(children.begin(), children.end());

  for (size_t i = 0; i < child_vec.size(); ++i) {
    if (has_returned_) return false;
    auto child = child_vec[i];
    auto ck = context_.node_kind(child);
    if (ck == Parse::NodeKind::CodeBlockStart) continue;
    // ExprStatement is a marker node (child_count=0); the expression
    // is the preceding sibling which we already evaluated.
    if (ck == Parse::NodeKind::ExprStatement) continue;

    // Condition-as-sibling pattern: an expression node that immediately
    // precedes IfStatement or WhileStatement is the condition for that
    // statement, not a standalone expression.
    // AssignmentExpr has the Expr category but is handled by EvalStmt,
    // so we exclude it from expression patterns here.
    if (ck.category().HasAnyOf(Parse::NodeCategory::Expr) &&
        ck != Parse::NodeKind::AssignmentExpr) {
      if (i + 1 < child_vec.size()) {
        auto next_kind = context_.node_kind(child_vec[i + 1]);
        if (next_kind == Parse::NodeKind::IfStatement ||
            next_kind == Parse::NodeKind::WhileStatement) {
          pending_condition_ = child;
          continue;
        }
        // Expression followed by ExprStatement marker: evaluate for
        // side effects (e.g., result.append(x)).
        if (next_kind == Parse::NodeKind::ExprStatement) {
          EvalExpr(child);
          if (has_returned_) return false;
          continue;
        }
      }
    }

    if (!EvalStmt(child)) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// EvalStmt — tree-walking statement evaluator
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalStmt(Parse::NodeId node_id) -> bool {
  if (!node_id.has_value()) return true;
  if (has_returned_) return false;
  if (!CheckIterationLimit(node_id)) return false;

  auto kind = context_.node_kind(node_id);

  // --- let / var declarations ---

  if (kind == Parse::NodeKind::LetDecl ||
      kind == Parse::NodeKind::VariableDecl) {
    auto children = context_.children_source_order(node_id);
    std::string var_name;
    Parse::NodeId init_node = Parse::NodeId::None;

    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::LetBindingPattern ||
          ck == Parse::NodeKind::VarBindingPattern ||
          ck == Parse::NodeKind::VariablePattern) {
        // Find the identifier name inside the pattern.
        auto pat_children = context_.children_source_order(child);
        for (auto pc : pat_children) {
          auto pk = context_.node_kind(pc);
          if (pk == Parse::NodeKind::IdentifierPattern ||
              pk == Parse::NodeKind::IdentifierNameNotBeforeParams ||
              pk == Parse::NodeKind::IdentifierNameBeforeParams ||
              pk == Parse::NodeKind::IdentifierNameExpr) {
            auto token = context_.node_token(pc);
            var_name = context_.token_text(token).str();
          }
        }
        // If the pattern itself is an identifier.
        if (var_name.empty()) {
          auto token = context_.node_token(child);
          var_name = context_.token_text(token).str();
        }
      } else if (ck == Parse::NodeKind::IdentifierPattern) {
        // Direct IdentifierPattern child (e.g., `var result: [Int] = []`).
        if (var_name.empty()) {
          auto token = context_.node_token(child);
          var_name = context_.token_text(token).str();
        }
      } else if (ck == Parse::NodeKind::LetIntroducer ||
                 ck == Parse::NodeKind::VariableIntroducer) {
        continue;
      } else if (ck == Parse::NodeKind::LetInitializer ||
                 ck == Parse::NodeKind::VariableInitializer) {
        continue;
      } else if (ck == Parse::NodeKind::TypeAnnotation) {
        continue;
      } else if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        init_node = child;
      }
    }

    if (!var_name.empty() && init_node.has_value()) {
      auto val = EvalExpr(init_node);
      if (val.has_value()) {
        SetVar(var_name, std::move(*val));
      }
    } else if (!var_name.empty()) {
      // Default-initialized variable (e.g., `var result: [Int] = []` — the
      // empty array literal should be caught by the expression path above).
      SetVar(var_name, ComptimeValue::MakeNone());
    }
    return true;
  }

  // --- Return statement ---

  if (kind == Parse::NodeKind::ReturnStatement) {
    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::ReturnStatementStart) continue;
      if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        auto val = EvalExpr(child);
        if (val.has_value()) {
          return_value_ = std::move(val);
        }
        has_returned_ = true;
        return false;
      }
    }
    // Void return.
    return_value_ = ComptimeValue::MakeNone();
    has_returned_ = true;
    return false;
  }

  // --- If statement ---

  if (kind == Parse::NodeKind::IfStatement) {
    // The condition expression is a preceding sibling (set by
    // EvalCodeBlockBody into pending_condition_), NOT a child of IfStatement.
    Parse::NodeId cond_node = Parse::NodeId::None;
    if (pending_condition_.has_value()) {
      cond_node = *pending_condition_;
      pending_condition_ = std::nullopt;
    }

    // IfStatement's own children: IfCondition (marker), CodeBlock(s),
    // optionally IfStatementElse + CodeBlock or nested IfStatement.
    Parse::NodeId then_block = Parse::NodeId::None;
    Parse::NodeId else_block = Parse::NodeId::None;

    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::IfCondition) continue;
      if (ck == Parse::NodeKind::IfStatementElse) continue;
      if (ck == Parse::NodeKind::CodeBlock) {
        if (!then_block.has_value()) {
          then_block = child;
        } else {
          else_block = child;
        }
      }
      // Nested if-else chain.
      if (ck == Parse::NodeKind::IfStatement) {
        else_block = child;
      }
    }

    if (!cond_node.has_value()) return true;
    auto cond = EvalExpr(cond_node);
    if (!cond.has_value()) return true;

    if (cond->ToBool()) {
      if (then_block.has_value()) {
        return EvalCodeBlockBody(then_block);
      }
    } else if (else_block.has_value()) {
      if (context_.node_kind(else_block) == Parse::NodeKind::IfStatement) {
        // else-if chain — condition should already be pending.
        return EvalStmt(else_block);
      }
      return EvalCodeBlockBody(else_block);
    }
    return true;
  }

  // --- While statement ---

  if (kind == Parse::NodeKind::WhileStatement) {
    // The condition expression is a preceding sibling (set by
    // EvalCodeBlockBody into pending_condition_).
    Parse::NodeId cond_node = Parse::NodeId::None;
    if (pending_condition_.has_value()) {
      cond_node = *pending_condition_;
      pending_condition_ = std::nullopt;
    }

    Parse::NodeId body_block = Parse::NodeId::None;
    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::WhileCondition) continue;
      if (ck == Parse::NodeKind::CodeBlock) {
        body_block = child;
      }
    }

    if (!cond_node.has_value() || !body_block.has_value()) return true;

    while (true) {
      if (!CheckIterationLimit(node_id)) return false;
      auto cond = EvalExpr(cond_node);
      if (!cond.has_value() || !cond->ToBool()) break;

      if (!EvalCodeBlockBody(body_block)) return false;
    }
    return true;
  }

  // --- For-in statement ---

  if (kind == Parse::NodeKind::ForInStatement) {
    auto children = context_.children_source_order(node_id);
    std::string loop_var;
    Parse::NodeId range_expr = Parse::NodeId::None;
    Parse::NodeId body_block = Parse::NodeId::None;

    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::ForInIntroducer) continue;
      if (ck == Parse::NodeKind::IdentifierPattern ||
          ck == Parse::NodeKind::IdentifierNameNotBeforeParams ||
          ck == Parse::NodeKind::IdentifierNameExpr) {
        if (loop_var.empty()) {
          auto token = context_.node_token(child);
          loop_var = context_.token_text(token).str();
        }
      }
      if (ck.category().HasAnyOf(Parse::NodeCategory::Expr) &&
          !range_expr.has_value() && loop_var.size() > 0) {
        range_expr = child;
      }
      if (ck == Parse::NodeKind::CodeBlock) {
        body_block = child;
      }
    }

    if (loop_var.empty() || !body_block.has_value()) return true;

    // Try to evaluate range expression.
    // For `0..<n` or similar, extract the InfixOperatorExpr with `..<`.
    if (range_expr.has_value()) {
      auto rk = context_.node_kind(range_expr);
      if (rk == Parse::NodeKind::InfixOperatorExpr) {
        auto range_token = context_.node_token(range_expr);
        auto range_op = context_.token_text(range_token);
        if (range_op == "..<" || range_op == "...") {
          auto range_children = context_.children_source_order(range_expr);
          Parse::NodeId lo_node = Parse::NodeId::None;
          Parse::NodeId hi_node = Parse::NodeId::None;
          for (auto rc : range_children) {
            if (context_.node_kind(rc).category().HasAnyOf(
                    Parse::NodeCategory::Expr)) {
              if (!lo_node.has_value())
                lo_node = rc;
              else
                hi_node = rc;
            }
          }
          auto lo_val = lo_node.has_value() ? EvalExpr(lo_node) : std::nullopt;
          auto hi_val = hi_node.has_value() ? EvalExpr(hi_node) : std::nullopt;
          if (lo_val.has_value() && hi_val.has_value()) {
            int64_t lo = lo_val->ToInt64();
            int64_t hi = hi_val->ToInt64();
            if (range_op == "...") hi += 1;  // closed range
            for (int64_t i = lo; i < hi; ++i) {
              if (!CheckIterationLimit(node_id)) return false;
              SetVarMutable(loop_var, ComptimeValue::MakeInt(i));
              if (!EvalCodeBlockBody(body_block)) return false;
            }
            return true;
          }
        }
      }
      // M114: For-in over array.
      auto range_val = EvalExpr(range_expr);
      if (range_val.has_value() && range_val->kind == ComptimeValue::Array) {
        for (auto& elem : range_val->array_vals) {
          if (!CheckIterationLimit(node_id)) return false;
          SetVarMutable(loop_var, elem);
          if (!EvalCodeBlockBody(body_block)) return false;
        }
        return true;
      }
    }
    return true;
  }

  // --- Expression statement ---

  if (kind == Parse::NodeKind::ExprStatement) {
    auto children = context_.children_source_order(node_id);
    for (auto child : children) {
      auto ck = context_.node_kind(child);
      if (ck == Parse::NodeKind::AssignmentExpr) {
        // Handle assignment: lhs = rhs
        return EvalAssignment(child);
      }
      if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        // Evaluate and discard (side-effect expression like method calls).
        auto val = EvalExpr(child);
        // M114: Check for .append() calls that modify mutable arrays.
        (void)val;
      }
    }
    return true;
  }

  // --- Assignment expression (when appearing as statement) ---

  if (kind == Parse::NodeKind::AssignmentExpr) {
    return EvalAssignment(node_id);
  }

  // --- Code block ---

  if (kind == Parse::NodeKind::CodeBlock) {
    return EvalCodeBlockBody(node_id);
  }

  // Unknown/unsupported statement kind at comptime — emit diagnostic.
  context_.EmitError(node_id, ComptimeUnsupportedOperation,
                     "unsupported statement kind in #comptime block");
  return false;
}

// ---------------------------------------------------------------------------
// Assignment helper
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::EvalAssignment(Parse::NodeId node_id) -> bool {
  auto children = context_.children_source_order(node_id);
  Parse::NodeId lhs_node = Parse::NodeId::None;
  Parse::NodeId rhs_node = Parse::NodeId::None;

  for (auto child : children) {
    if (context_.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      if (!lhs_node.has_value())
        lhs_node = child;
      else
        rhs_node = child;
    }
  }

  if (!lhs_node.has_value() || !rhs_node.has_value()) return true;

  auto rhs = EvalExpr(rhs_node);
  if (!rhs.has_value()) return true;

  // Simple variable assignment.
  auto lhs_kind = context_.node_kind(lhs_node);
  if (lhs_kind == Parse::NodeKind::IdentifierNameExpr) {
    auto token = context_.node_token(lhs_node);
    auto name = context_.token_text(token);
    SetVarMutable(name, std::move(*rhs));
    return true;
  }

  // Array subscript assignment: arr[i] = val
  if (lhs_kind == Parse::NodeKind::SubscriptExpr) {
    auto sub_children = context_.children_source_order(lhs_node);
    Parse::NodeId base_node = Parse::NodeId::None;
    Parse::NodeId index_node = Parse::NodeId::None;
    for (auto sc : sub_children) {
      auto sk = context_.node_kind(sc);
      // Base expression is inside SubscriptExprStart.
      if (sk == Parse::NodeKind::SubscriptExprStart) {
        auto start_children = context_.children_source_order(sc);
        for (auto ssc : start_children) {
          if (context_.node_kind(ssc).category().HasAnyOf(
                  Parse::NodeCategory::Expr)) {
            base_node = ssc;
          }
        }
        continue;
      }
      if (sk.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        index_node = sc;
      }
    }
    if (base_node.has_value() && index_node.has_value()) {
      auto idx = EvalExpr(index_node);
      if (idx.has_value() && idx->kind == ComptimeValue::Int) {
        // Get the array variable name.
        if (context_.node_kind(base_node) ==
            Parse::NodeKind::IdentifierNameExpr) {
          auto token = context_.node_token(base_node);
          auto arr_name = context_.token_text(token);
          auto arr_val = GetVar(arr_name);
          if (arr_val.has_value() &&
              arr_val->kind == ComptimeValue::Array) {
            auto i = idx->int_val;
            if (i >= 0 &&
                i < static_cast<int64_t>(arr_val->array_vals.size())) {
              arr_val->array_vals[static_cast<size_t>(i)] = std::move(*rhs);
              SetVarMutable(arr_name, std::move(*arr_val));
            }
          }
        }
      }
    }
    return true;
  }

  // Struct field assignment: s.field = val
  if (lhs_kind == Parse::NodeKind::MemberAccessExpr) {
    auto mem_children = context_.children_source_order(lhs_node);
    Parse::NodeId base_node = Parse::NodeId::None;
    for (auto mc : mem_children) {
      if (context_.node_kind(mc).category().HasAnyOf(
              Parse::NodeCategory::Expr)) {
        base_node = mc;
        break;
      }
    }
    if (base_node.has_value() &&
        context_.node_kind(base_node) ==
            Parse::NodeKind::IdentifierNameExpr) {
      auto base_token = context_.node_token(base_node);
      auto base_name = context_.token_text(base_token);
      auto member_token = context_.node_token(lhs_node);
      auto member_name = context_.token_text(member_token);

      auto struct_val = GetVar(base_name);
      if (struct_val.has_value() &&
          struct_val->kind == ComptimeValue::Struct) {
        for (auto& [fname, fval] : struct_val->struct_fields) {
          if (fname == member_name) {
            fval = std::move(*rhs);
            SetVarMutable(base_name, std::move(*struct_val));
            return true;
          }
        }
      }
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// ValueToSemIR — convert comptime result to SemIR constant instruction
// ---------------------------------------------------------------------------

auto ComptimeEvaluator::ValueToSemIR(Parse::NodeId loc,
                                      const ComptimeValue& val)
    -> SemIR::InstId {
  switch (val.kind) {
    case ComptimeValue::Int: {
      auto int_id = context_.ints().Add(val.int_val);
      auto type_id = context_.GetBuiltinType("Int");
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::IntValue{.type_id = type_id, .int_id = int_id}));
    }
    case ComptimeValue::Bool: {
      auto type_id = context_.GetBuiltinType("Bool");
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::BoolLiteral{.type_id = type_id,
                             .value = SemIR::BoolValue::From(val.bool_val)}));
    }
    case ComptimeValue::Float: {
      llvm::APFloat ap_val(val.float_val);
      auto float_id = context_.floats().Add(ap_val);
      auto type_id = context_.GetBuiltinType("Double");
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::FloatValue{.type_id = type_id, .float_id = float_id}));
    }
    case ComptimeValue::String: {
      auto string_id =
          context_.string_literal_values().Add(val.string_val);
      auto type_id = context_.GetBuiltinType("String");
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::StringLiteral{.type_id = type_id,
                               .string_id = string_id}));
    }
    case ComptimeValue::Array: {
      // Emit each element, collect InstIds, create ArrayLiteralInit.
      llvm::SmallVector<SemIR::InstId> elem_inst_ids;
      SemIR::TypeId elem_type_id = context_.GetBuiltinType("Int");  // default
      for (auto& elem : val.array_vals) {
        auto elem_id = ValueToSemIR(loc, elem);
        if (elem_id.has_value()) {
          elem_inst_ids.push_back(elem_id);
          // Use the type of the first element.
          if (elem_inst_ids.size() == 1) {
            auto inst = context_.insts().Get(elem_id);
            elem_type_id = inst.type_id();
          }
        }
      }
      auto elems_block_id =
          context_.inst_blocks().AddPlaceholder();
      context_.inst_blocks().ReplacePlaceholder(elems_block_id,
                                                 elem_inst_ids);
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::ArrayLiteralInit{.type_id = elem_type_id,
                                   .elements_id = elems_block_id}));
    }
    case ComptimeValue::Struct: {
      // Emit each field value, create StructInit.
      llvm::SmallVector<SemIR::InstId> field_inst_ids;
      for (auto& [fname, fval] : val.struct_fields) {
        auto fid = ValueToSemIR(loc, fval);
        if (fid.has_value()) {
          field_inst_ids.push_back(fid);
        }
      }
      // Look up the struct TypeId.
      auto ident_id =
          context_.identifiers().Add(val.struct_type_name);
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      auto type_inst_id = context_.LookupName(name_id);
      auto type_id = SemIR::TypeId::None;
      if (type_inst_id.has_value()) {
        type_id = SemIR::TypeId::ForTypeConstant(
            SemIR::ConstantId::ForConcreteConstant(type_inst_id));
      }

      auto args_block_id =
          context_.inst_blocks().AddPlaceholder();
      context_.inst_blocks().ReplacePlaceholder(args_block_id,
                                                 field_inst_ids);
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::StructInit{.type_id = type_id,
                             .args_id = args_block_id}));
    }
    case ComptimeValue::None:
    default: {
      // Return a zero int as fallback.
      auto int_id = context_.ints().Add(0);
      auto type_id = context_.GetBuiltinType("Int");
      return context_.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(loc),
          SemIR::IntValue{.type_id = type_id, .int_id = int_id}));
    }
  }
}

}  // namespace TinySwift::Check
