// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_expr.h"

#include "toolchain/check/handle_generic.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// M68/M72: Diagnostic for failed generic type inference.
TINYSWIFT_DIAGNOSTIC(CannotInferGenericTypeArgs, Error,
                     "cannot infer generic type arguments for '{0}'",
                     std::string);

// Helper to get the type of an instruction.
auto GetInstType(Context& context, SemIR::InstId inst_id) -> SemIR::TypeId {
  if (!inst_id.has_value()) {
    return SemIR::ErrorInst::TypeId;
  }
  return context.insts().Get(inst_id).type_id();
}

// Returns true if the type is Float or Double.
auto IsFloatType(Context& context, SemIR::TypeId type_id) -> bool {
  if (!type_id.has_value() || !type_id.is_concrete()) {
    return false;
  }
  auto float_type = context.GetBuiltinType("Float");
  auto double_type = context.GetBuiltinType("Double");
  return type_id == float_type || type_id == double_type;
}

// Returns true if the type is Int (IntLiteralType).
auto IsIntType(Context& context, SemIR::TypeId type_id) -> bool {
  if (!type_id.has_value() || !type_id.is_concrete()) {
    return false;
  }
  auto int_type = context.GetBuiltinType("Int");
  return type_id == int_type;
}

// Returns true if the type is Bool.
auto IsBoolType(Context& context, SemIR::TypeId type_id) -> bool {
  if (!type_id.has_value() || !type_id.is_concrete()) {
    return false;
  }
  auto bool_type = context.GetBuiltinType("Bool");
  return type_id == bool_type;
}

// Returns true if the type is String.
auto IsStringType(Context& context, SemIR::TypeId type_id) -> bool {
  if (!type_id.has_value() || !type_id.is_concrete()) {
    return false;
  }
  auto string_type = context.GetBuiltinType("String");
  return type_id == string_type;
}

// Returns true if the type is an error type.
auto IsErrorType(SemIR::TypeId type_id) -> bool {
  return !type_id.has_value() || type_id == SemIR::ErrorInst::TypeId;
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

// Handles a string literal, including M38 string interpolation `"text\(expr)"`.
// Interpolated segments are desugared into IntToString + StringConcat chains.
auto HandleStringLiteral(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  // Strip surrounding quotes.
  llvm::StringRef content = text;
  if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
    content = content.drop_front(1).drop_back(1);
  }

  auto string_type_id = context.GetBuiltinType("String");
  auto int_type_id = context.GetBuiltinType("Int");

  // Check for string interpolation: `\(...)` sequences.
  // Only handle simple `\(identifier)` for M38.
  if (!content.contains("\\(")) {
    // Plain string literal — no interpolation.
    auto string_id = context.string_literal_values().Add(content);
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::StringLiteral{.type_id = string_type_id,
                             .string_id = string_id}));
  }

  // Desugar string interpolation into literal + IntToString + StringConcat.
  // Split at `\(...)` boundaries.
  SemIR::InstId result_id = SemIR::InstId::None;

  llvm::StringRef remaining = content;
  while (!remaining.empty()) {
    auto interp_pos = remaining.find("\\(");
    if (interp_pos == llvm::StringRef::npos) {
      // Trailing literal segment.
      auto seg_id = context.string_literal_values().Add(remaining);
      auto seg_inst = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::StringLiteral{.type_id = string_type_id,
                               .string_id = seg_id}));
      if (!result_id.has_value()) {
        result_id = seg_inst;
      } else {
        result_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringConcat{.type_id = string_type_id,
                                .lhs_id = result_id,
                                .rhs_id = seg_inst}));
      }
      break;
    }

    // Emit the literal part before `\(`.
    if (interp_pos > 0) {
      auto literal_part = remaining.take_front(interp_pos);
      auto seg_id = context.string_literal_values().Add(literal_part);
      auto seg_inst = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::StringLiteral{.type_id = string_type_id,
                               .string_id = seg_id}));
      if (!result_id.has_value()) {
        result_id = seg_inst;
      } else {
        result_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringConcat{.type_id = string_type_id,
                                .lhs_id = result_id,
                                .rhs_id = seg_inst}));
      }
    }

    // Find closing `)` of `\(...)`.
    remaining = remaining.drop_front(interp_pos + 2);  // skip `\(`
    size_t depth = 1;
    size_t close_pos = 0;
    while (close_pos < remaining.size() && depth > 0) {
      if (remaining[close_pos] == '(') ++depth;
      else if (remaining[close_pos] == ')') --depth;
      if (depth > 0) ++close_pos;
    }
    auto expr_text = remaining.take_front(close_pos);
    remaining = remaining.drop_front(close_pos + 1);  // skip past `)`

    // Look up the expression by name in scope.
    // M38 supports simple identifier interpolation only.
    auto ident_id = context.identifiers().Lookup(expr_text);
    SemIR::InstId interp_inst = SemIR::InstId::None;
    if (ident_id.has_value()) {
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      auto value_id = context.LookupName(name_id);
      if (value_id.has_value()) {
        auto ref_type_id = GetInstType(context, value_id);
        auto name_ref_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::NameRef{.type_id = ref_type_id,
                           .name_id = name_id,
                           .value_id = value_id}));
        // Convert to String: Int/Bool → IntToString, String stays as-is.
        if (ref_type_id == int_type_id ||
            ref_type_id == context.GetBuiltinType("Bool")) {
          interp_inst = context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::IntToString{.type_id = string_type_id,
                                 .operand_id = name_ref_id}));
        } else {
          interp_inst = name_ref_id;  // Already a String.
        }
      }
    }

    if (interp_inst.has_value()) {
      if (!result_id.has_value()) {
        result_id = interp_inst;
      } else {
        result_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringConcat{.type_id = string_type_id,
                                .lhs_id = result_id,
                                .rhs_id = interp_inst}));
      }
    }
  }

  if (!result_id.has_value()) {
    // Fallback: empty string.
    auto empty_id = context.string_literal_values().Add("");
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::StringLiteral{.type_id = string_type_id,
                             .string_id = empty_id}));
  }
  return result_id;
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
    context.EmitError(node_id, UndefinedName, std::string(text));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto value_id = context.LookupName(name_id);
  if (!value_id.has_value()) {
    context.EmitError(node_id, UndefinedName, std::string(text));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // Implicit self resolution: if the resolved value is a type-scope member
  // (StructField, ComputedPropertyDecl, FunctionDecl from type scope) and
  // we're inside a type context with `self` available, synthesize
  // `self.<member>` instead of a bare NameRef to the member inst.
  auto resolved_inst = context.insts().Get(value_id);
  if (context.CurrentTypeInstId().has_value()) {
    auto self_ident = context.identifiers().Lookup("self");
    SemIR::InstId self_id = SemIR::InstId::None;
    if (self_ident.has_value()) {
      auto self_name = SemIR::NameId::ForIdentifier(self_ident);
      self_id = context.LookupName(self_name);
    }
    if (self_id.has_value()) {
      if (auto field = resolved_inst.TryAs<SemIR::StructField>()) {
        // Emit NameRef for self, then FieldAccess.
        auto self_type = GetInstType(context, self_id);
        auto self_ref_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::NameRef{.type_id = self_type,
                           .name_id = SemIR::NameId::ForIdentifier(
                               self_ident),
                           .value_id = self_id}));
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FieldAccess{.type_id = field->type_id,
                               .base_id = self_ref_id,
                               .index = field->index}));
      }
      if (auto cpd = resolved_inst.TryAs<SemIR::ComputedPropertyDecl>()) {
        auto self_type = GetInstType(context, self_id);
        auto self_ref_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::NameRef{.type_id = self_type,
                           .name_id = SemIR::NameId::ForIdentifier(
                               self_ident),
                           .value_id = self_id}));
        auto args_block_id = context.inst_blocks().AddPlaceholder();
        context.inst_blocks().ReplacePlaceholder(
            args_block_id,
            llvm::ArrayRef<SemIR::InstId>{self_ref_id});
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Call{.type_id = cpd->type_id,
                        .callee_id = cpd->getter_id,
                        .args_id = args_block_id}));
      }
      if (auto fn_decl = resolved_inst.TryAs<SemIR::FunctionDecl>()) {
        auto& fn = context.functions().Get(fn_decl->function_id);
        // Only treat as implicit self method if the function was declared
        // in the current type's scope (not a free function from an outer
        // scope that happens to be visible).
        bool is_type_member = false;
        if (!fn.is_static) {
          auto enclosing_type = context.CurrentTypeInstId();
          auto type_inst = context.insts().Get(enclosing_type);
          SemIR::NameScopeId type_scope_id = SemIR::NameScopeId::None;
          if (auto st = type_inst.TryAs<SemIR::StructType>()) {
            type_scope_id = st->name_scope_id;
          } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
            type_scope_id = ct->name_scope_id;
          } else if (auto ed = type_inst.TryAs<SemIR::EnumDecl>()) {
            type_scope_id = ed->name_scope_id;
          } else {
            // M88: Built-in type extension scope.
            type_scope_id = context.GetBuiltinTypeScope(enclosing_type);
          }
          is_type_member = type_scope_id.has_value() &&
                           fn.parent_scope_id == type_scope_id;
        }
        if (is_type_member) {
          auto self_type = GetInstType(context, self_id);
          auto self_ref_id = context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::NameRef{.type_id = self_type,
                             .name_id = SemIR::NameId::ForIdentifier(
                                 self_ident),
                             .value_id = self_id}));
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::BoundMethod{.type_id = SemIR::TypeType::TypeId,
                                 .base_id = self_ref_id,
                                 .function_id = value_id}));
        }
      }
    }
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
  auto children = context.children_source_order(node_id);
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
  auto children = context.children_source_order(node_id);

  SemIR::InstId callee_id = SemIR::InstId::None;
  Parse::NodeId call_start_node = Parse::NodeId::None;  // M68: for generic arg extraction

  // Labeled arguments: (label_text, value_inst_id).
  struct LabeledArg {
    llvm::StringRef label;
    SemIR::InstId value_id;
  };
  llvm::SmallVector<LabeledArg> labeled_args;
  llvm::StringRef pending_label;

  // Detect builtin type coercion calls: Int(expr) and Double(expr).
  // Check the callee name before calling HandleExpr to avoid spurious errors.
  llvm::StringRef coercion_target;
  bool is_print_call = false;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::CallExprStart) {
      call_start_node = child;  // M68: save for generic arg extraction
      // CallExprStart has the callee as its child (non-generic case).
      // In generic calls like `identity<Int>(42)`, the callee IdentifierNameExpr
      // is a SIBLING of CallExprStart (not its child) because GenericArgumentClause
      // shifts the subtree boundary. Check if callee was already found as sibling.
      if (!callee_id.has_value() && !coercion_target.empty() == false &&
          !is_print_call) {
        auto start_children = context.children_source_order(child);
        for (auto start_child : start_children) {
          if (context.node_kind(start_child)
                  .category()
                  .HasAnyOf(Parse::NodeCategory::Expr)) {
            // Check for Int(...) / Double(...) type coercions before lookup.
            if (context.node_kind(start_child) ==
                Parse::NodeKind::IdentifierNameExpr) {
              auto text = context.token_text(context.node_token(start_child));
              if (text == "Int" || text == "Double") {
                coercion_target = text;
                break;  // Don't call HandleExpr — skip normal callee resolution.
              }
              // M44: Intercept print() before normal lookup.
              if (text == "print") {
                is_print_call = true;
                break;
              }
            }
            callee_id = HandleExpr(context, start_child);
            break;
          }
        }
      }
    } else if (child_kind == Parse::NodeKind::ArgumentLabel) {
      // Record the label for the next argument.
      pending_label = context.token_text(context.node_token(child));
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // M68: Before CallExprStart is seen, an Expr child is the callee
      // (happens in generic calls where the callee is a sibling of CallExprStart).
      // After CallExprStart, Expr children are arguments.
      if (!call_start_node.has_value() && !callee_id.has_value()) {
        // This is the callee expression (before CallExprStart).
        // Check for Int/Double/print interception.
        if (child_kind == Parse::NodeKind::IdentifierNameExpr) {
          auto text = context.token_text(context.node_token(child));
          if (text == "Int" || text == "Double") {
            coercion_target = text;
            continue;
          }
          if (text == "print") {
            is_print_call = true;
            continue;
          }
        }
        callee_id = HandleExpr(context, child);
      } else {
        // Argument expression — associate with pending label.
        labeled_args.push_back({pending_label, HandleExpr(context, child)});
        pending_label = llvm::StringRef();
      }
    }
  }

  // M68: If callee was not found inside CallExpr's subtree (generic calls shift
  // the IdentifierNameExpr outside), pick up the pending callee set by the
  // parent context (HandleReturnStatement or HandleCodeBlock).
  if (!callee_id.has_value() && coercion_target.empty() && !is_print_call) {
    callee_id = context.TakePendingCalleeId();
  }

  // M44: Handle print(expr) — emit PrintValue for any single argument.
  if (is_print_call) {
    SemIR::InstId arg_id = SemIR::InstId::None;
    if (!labeled_args.empty()) {
      arg_id = labeled_args[0].value_id;
    }
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::PrintValue{.type_id = SemIR::ErrorInst::TypeId,
                          .arg_id = arg_id}));
  }

  // Handle builtin type coercions: Int(floatExpr) and Double(intExpr).
  if (!coercion_target.empty() && labeled_args.size() == 1) {
    auto arg_id = labeled_args[0].value_id;
    if (arg_id.has_value()) {
      auto arg_type = context.insts().Get(arg_id).type_id();
      if (coercion_target == "Int" && IsFloatType(context, arg_type)) {
        auto int_type = context.GetBuiltinType("Int");
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatToInt{.type_id = int_type, .operand_id = arg_id}));
      }
      if (coercion_target == "Double" && IsIntType(context, arg_type)) {
        auto double_type = context.GetBuiltinType("Double");
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntToFloat{.type_id = double_type, .operand_id = arg_id}));
      }
    }
    // Coercion target name was recognized but arg type didn't match.
    context.EmitError(node_id, UndefinedName, std::string(coercion_target));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // Determine return type, validate arguments, and build ordered arg list.
  auto type_id = SemIR::ErrorInst::TypeId;
  llvm::SmallVector<SemIR::InstId> ordered_args;

  if (callee_id.has_value()) {
    // Look through NameRef to find the actual callee instruction.
    SemIR::InstId actual_callee_id = callee_id;
    auto callee_inst = context.insts().Get(callee_id);
    if (auto name_ref = callee_inst.TryAs<SemIR::NameRef>()) {
      actual_callee_id = name_ref->value_id;
      callee_inst = context.insts().Get(name_ref->value_id);
    }
    // Also look through ValueBinding (e.g. `let f = { x in ... }` binds a
    // ValueBinding whose value_id is the ClosureExpr).
    if (auto value_binding = callee_inst.TryAs<SemIR::ValueBinding>()) {
      if (value_binding->value_id.has_value()) {
        actual_callee_id = value_binding->value_id;
        callee_inst = context.insts().Get(value_binding->value_id);
      }
    }

    // Enum case with payload: `Result.success(42)` → TupleInit{tag, payload}.
    if (auto ecwp = callee_inst.TryAs<SemIR::EnumCaseWithPayload>()) {
      type_id = ecwp->type_id;
      auto int_type = context.GetBuiltinType("Int");
      // Emit an integer constant for the discriminant tag.
      auto disc_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntValue{.type_id = int_type,
                          .int_id = context.ints().Add(
                              ecwp->discriminant.index)}));
      // The payload is the first (and only) call argument.
      SemIR::InstId payload_id = SemIR::InstId::None;
      if (!labeled_args.empty()) {
        payload_id = labeled_args[0].value_id;
      }
      llvm::SmallVector<SemIR::InstId> elems{disc_id};
      if (payload_id.has_value()) {
        elems.push_back(payload_id);
      }
      auto block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          block_id, llvm::ArrayRef<SemIR::InstId>(elems));
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::TupleInit{.type_id = type_id, .elements_id = block_id}));
    }

    // M61: String method call — resolve StringMethodRef pending marker.
    if (auto smr = callee_inst.TryAs<SemIR::StringMethodRef>()) {
      auto bool_type = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));
      SemIR::InstId arg_id =
          labeled_args.empty() ? SemIR::InstId::None : labeled_args[0].value_id;
      auto method_ident_id = smr->method_name_id.AsIdentifierId();
      if (method_ident_id.has_value()) {
        auto method_text = context.identifiers().Get(method_ident_id);
        if (method_text == "hasPrefix") {
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::StringHasPrefix{.type_id = bool_type,
                                     .str_id = smr->str_id,
                                     .arg_id = arg_id}));
        }
        if (method_text == "hasSuffix") {
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::StringHasSuffix{.type_id = bool_type,
                                     .str_id = smr->str_id,
                                     .arg_id = arg_id}));
        }
        if (method_text == "contains") {
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::StringContains{.type_id = bool_type,
                                    .str_id = smr->str_id,
                                    .arg_id = arg_id}));
        }
      }
    }

    // M65: Dynamic array method call — resolve DynamicArrayMethodRef.
    if (auto dam = callee_inst.TryAs<SemIR::DynamicArrayMethodRef>()) {
      if (labeled_args.empty()) {
        context.EmitError(node_id, TooFewArguments, 1, 0);
        return context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
      }
      SemIR::InstId elem_id = labeled_args[0].value_id;
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::DynamicArrayAppend{.type_id = SemIR::ErrorInst::TypeId,
                                    .array_id = dam->array_id,
                                    .elem_id = elem_id}));
    }

    // M77: UnsafePointer method call — resolve UnsafePtrMethodRef.
    if (auto upm = callee_inst.TryAs<SemIR::UnsafePtrMethodRef>()) {
      auto method_ident = upm->method_name_id.AsIdentifierId();
      llvm::StringRef method_text = method_ident.has_value()
          ? context.identifiers().Get(method_ident) : llvm::StringRef();
      if (method_text == "deallocate") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::UnsafePtrDeallocate{.type_id = SemIR::ErrorInst::TypeId,
                                       .ptr_id = upm->ptr_id}));
      }
      if (method_text == "allocate") {
        // Static call: UnsafeMutablePointer<Int>.allocate(capacity: N)
        auto ptr_type = GetInstType(context, upm->ptr_id);
        SemIR::InstId capacity_id = SemIR::InstId::None;
        if (!labeled_args.empty()) {
          capacity_id = labeled_args[0].value_id;
        } else {
          // Default capacity 1.
          auto int_type = context.GetBuiltinType("Int");
          capacity_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
              SemIR::IntValue{.type_id = int_type,
                              .int_id = context.ints().Add(1LL)}));
        }
        auto int_type = context.GetBuiltinType("Int");
        // Element size = 8 bytes (Int = i64).
        auto elem_size_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::IntValue{.type_id = int_type,
                            .int_id = context.ints().Add(8LL)}));
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::UnsafePtrAllocate{.type_id = ptr_type,
                                     .capacity_id = capacity_id,
                                     .elem_size_id = elem_size_id}));
      }
    }

    if (auto bm = callee_inst.TryAs<SemIR::BoundMethod>()) {
      // --- Method call: p.method(args...) → MethodName(self: p, args...) ---
      auto fn_decl_inst = context.insts().Get(bm->function_id);
      if (auto fn_decl = fn_decl_inst.TryAs<SemIR::FunctionDecl>()) {
        auto& fn = context.functions().Get(fn_decl->function_id);
        if (fn.return_type_inst_id.has_value()) {
          type_id = context.types().GetTypeIdForTypeInstId(
              fn.return_type_inst_id);
        }

        // The self arg is the base object (e.g., NameRef pointing to p).
        // M56/M54: Mutating methods need self as a pointer (AddressOf VarStorage).
        auto self_arg_id = bm->base_id;
        if (fn.is_mutating) {
          // Chase NameRef → VarStorage, then emit AddressOf.
          auto base_inst = context.insts().Get(bm->base_id);
          SemIR::InstId var_id = SemIR::InstId::None;
          if (auto nr = base_inst.TryAs<SemIR::NameRef>()) {
            auto inner = context.insts().Get(nr->value_id);
            if (inner.Is<SemIR::VarStorage>()) {
              var_id = nr->value_id;
            }
          } else if (base_inst.Is<SemIR::VarStorage>()) {
            var_id = bm->base_id;
          }
          if (var_id.has_value()) {
            self_arg_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::AddressOf{.type_id = SemIR::ErrorInst::TypeId,
                                 .operand_id = var_id}));
          }
        }

        if (fn.call_params_id.has_value() &&
            fn.call_params_id != SemIR::InstBlockId::Empty) {
          auto param_ids =
              context.sem_ir().inst_blocks().Get(fn.call_params_id);
          int total_params = static_cast<int>(param_ids.size());
          int user_params = total_params - 1;  // exclude self at index 0

          ordered_args.resize(total_params, SemIR::InstId::None);
          ordered_args[0] = self_arg_id;  // self is always first

          // Build param name list for regular params (indices 1..N).
          llvm::SmallVector<llvm::StringRef> param_names;
          for (int i = 1; i < total_params; ++i) {
            auto param_inst = context.insts().Get(param_ids[i]);
            if (auto vp = param_inst.TryAs<SemIR::ValueParam>()) {
              auto ident_id = vp->pretty_name_id.AsIdentifierId();
              if (ident_id.has_value()) {
                param_names.push_back(context.identifiers().Get(ident_id));
              } else {
                param_names.push_back(llvm::StringRef());
              }
            } else {
              param_names.push_back(llvm::StringRef());
            }
          }

          // Match labeled args to regular params.
          llvm::SmallVector<bool> param_filled(total_params, false);
          param_filled[0] = true;  // self already filled

          for (auto& arg : labeled_args) {
            if (!arg.label.empty()) {
              bool found = false;
              for (int j = 0; j < user_params; ++j) {
                if (param_names[j] == arg.label) {
                  if (!param_filled[j + 1]) {
                    ordered_args[j + 1] = arg.value_id;
                    param_filled[j + 1] = true;
                  }
                  found = true;
                  break;
                }
              }
              if (!found) {
                context.EmitError(node_id, ArgumentLabelMismatch,
                                  std::string(arg.label));
              }
            }
          }

          // Place unlabeled args in first unfilled regular-param slot.
          int next_positional = 1;
          for (auto& arg : labeled_args) {
            if (arg.label.empty()) {
              while (next_positional < total_params &&
                     param_filled[next_positional]) {
                ++next_positional;
              }
              if (next_positional < total_params) {
                ordered_args[next_positional] = arg.value_id;
                param_filled[next_positional] = true;
                ++next_positional;
              }
            }
          }

          int actual_count = static_cast<int>(labeled_args.size());
          if (actual_count > user_params) {
            context.EmitError(node_id, TooManyArguments,
                              user_params, actual_count);
          } else if (actual_count < user_params) {
            context.EmitError(node_id, TooFewArguments,
                              user_params, actual_count);
          }
        } else {
          // Zero-param method: args = [self].
          ordered_args.push_back(self_arg_id);
        }

        // Build the arg block and emit Call with the FunctionDecl as callee.
        llvm::SmallVector<SemIR::InstId> valid_args;
        for (auto arg : ordered_args) {
          if (arg.has_value()) valid_args.push_back(arg);
        }
        auto args_block_id = context.inst_blocks().AddPlaceholder();
        context.inst_blocks().ReplacePlaceholder(
            args_block_id, llvm::ArrayRef<SemIR::InstId>(valid_args));
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Call{.type_id = type_id,
                        .callee_id = bm->function_id,
                        .args_id = args_block_id}));
      }
    }

    if (auto fn_decl = callee_inst.TryAs<SemIR::FunctionDecl>()) {
      auto& fn = context.functions().Get(fn_decl->function_id);
      // M68: Check if this is a generic function template.
      if (fn.is_generic_template) {
        // Extract explicit type args from CallExprStart's GenericArgumentClause.
        auto type_args = ExtractGenericArgs(context, call_start_node);

        // M72: If no explicit type args, try to infer them.
        if (type_args.empty()) {
          llvm::SmallVector<std::pair<llvm::StringRef, SemIR::InstId>> inference_args;
          for (auto& arg : labeled_args) {
            inference_args.push_back({arg.label, arg.value_id});
          }
          type_args = InferTypeArgs(context, fn, inference_args);
          if (type_args.empty()) {
            auto fn_name = fn.name_id.has_value() && fn.name_id.AsIdentifierId().has_value()
                ? context.identifiers().Get(fn.name_id.AsIdentifierId()).str()
                : std::string("<unknown>");
            context.EmitError(node_id, CannotInferGenericTypeArgs, fn_name);
            return context.AddInst(SemIR::LocIdAndInst::NoLoc(
                SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
          }
        }

        // Specialize the generic function.
        auto specialized_fn_id = SpecializeGenericFunction(
            context, fn_decl->function_id, type_args);

        // Now emit a Call instruction to the specialized function.
        auto& spec_fn = context.functions().Get(specialized_fn_id);
        auto spec_type_id = SemIR::ErrorInst::TypeId;
        if (spec_fn.return_type_inst_id.has_value()) {
          spec_type_id = context.types().GetTypeIdForTypeInstId(
              spec_fn.return_type_inst_id);
        }

        // Find the FunctionDecl InstId for the specialized function.
        // Scan insts to find a FunctionDecl pointing to specialized_fn_id.
        // CRITICAL: Must use GetIdTag().Apply() to create properly tagged InstIds.
        SemIR::InstId spec_fn_decl_id = SemIR::InstId::None;
        auto inst_tag = context.insts().GetIdTag();
        int num_insts = context.insts().size();
        for (int idx = num_insts - 1; idx >= 0; --idx) {
          auto tagged_id = inst_tag.Apply(idx);
          auto inst = context.insts().Get(tagged_id);
          if (auto fd = inst.TryAs<SemIR::FunctionDecl>()) {
            if (fd->function_id == specialized_fn_id) {
              spec_fn_decl_id = tagged_id;
              break;
            }
          }
        }

        // Build args block with label matching against specialized function.
        llvm::SmallVector<SemIR::InstId> spec_ordered_args;
        int spec_expected_count = 0;
        if (spec_fn.call_params_id.has_value() &&
            spec_fn.call_params_id != SemIR::InstBlockId::Empty) {
          auto param_ids = context.sem_ir().inst_blocks().Get(spec_fn.call_params_id);
          spec_expected_count = static_cast<int>(param_ids.size());

          llvm::SmallVector<llvm::StringRef> param_names;
          for (auto param_id : param_ids) {
            auto param_inst = context.insts().Get(param_id);
            if (auto vp = param_inst.TryAs<SemIR::ValueParam>()) {
              auto ident_id = vp->pretty_name_id.AsIdentifierId();
              if (ident_id.has_value()) {
                param_names.push_back(context.identifiers().Get(ident_id));
              } else {
                param_names.push_back(llvm::StringRef());
              }
            } else {
              param_names.push_back(llvm::StringRef());
            }
          }

          spec_ordered_args.resize(spec_expected_count, SemIR::InstId::None);
          llvm::SmallVector<bool> param_filled(spec_expected_count, false);

          for (auto& arg : labeled_args) {
            if (!arg.label.empty()) {
              for (int j = 0; j < spec_expected_count; ++j) {
                if (param_names[j] == arg.label && !param_filled[j]) {
                  spec_ordered_args[j] = arg.value_id;
                  param_filled[j] = true;
                  break;
                }
              }
            }
          }
          int next_pos = 0;
          for (auto& arg : labeled_args) {
            if (arg.label.empty()) {
              while (next_pos < spec_expected_count && param_filled[next_pos]) ++next_pos;
              if (next_pos < spec_expected_count) {
                spec_ordered_args[next_pos] = arg.value_id;
                param_filled[next_pos] = true;
                ++next_pos;
              }
            }
          }
        } else {
          for (auto& arg : labeled_args) {
            spec_ordered_args.push_back(arg.value_id);
          }
        }

        llvm::SmallVector<SemIR::InstId> valid_args;
        for (auto a : spec_ordered_args) {
          if (a.has_value()) valid_args.push_back(a);
        }
        auto args_block_id = context.inst_blocks().AddPlaceholder();
        context.inst_blocks().ReplacePlaceholder(
            args_block_id, llvm::ArrayRef<SemIR::InstId>(valid_args));

        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Call{.type_id = spec_type_id,
                        .callee_id = spec_fn_decl_id.has_value()
                            ? spec_fn_decl_id : actual_callee_id,
                        .args_id = args_block_id}));
      }

      // --- Non-generic function call with label matching ---
      if (fn.return_type_inst_id.has_value()) {
        type_id = context.types().GetTypeIdForTypeInstId(
            fn.return_type_inst_id);
      }

      int expected_count = 0;
      if (fn.call_params_id.has_value() &&
          fn.call_params_id != SemIR::InstBlockId::Empty) {
        auto param_ids =
            context.sem_ir().inst_blocks().Get(fn.call_params_id);
        expected_count = static_cast<int>(param_ids.size());

        // Build parameter name list for label matching.
        llvm::SmallVector<llvm::StringRef> param_names;
        for (auto param_id : param_ids) {
          auto param_inst = context.insts().Get(param_id);
          if (auto vp = param_inst.TryAs<SemIR::ValueParam>()) {
            auto ident_id = vp->pretty_name_id.AsIdentifierId();
            if (ident_id.has_value()) {
              param_names.push_back(context.identifiers().Get(ident_id));
            } else {
              param_names.push_back(llvm::StringRef());
            }
          } else {
            param_names.push_back(llvm::StringRef());
          }
        }

        // Match labeled args to parameters.
        ordered_args.resize(expected_count, SemIR::InstId::None);
        llvm::SmallVector<bool> param_filled(expected_count, false);

        // First pass: place labeled args by name.
        for (auto& arg : labeled_args) {
          if (!arg.label.empty()) {
            bool found = false;
            for (int j = 0; j < expected_count; ++j) {
              if (param_names[j] == arg.label) {
                if (!param_filled[j]) {
                  ordered_args[j] = arg.value_id;
                  param_filled[j] = true;
                }
                found = true;
                break;
              }
            }
            if (!found) {
              context.EmitError(node_id, ArgumentLabelMismatch,
                                std::string(arg.label));
            }
          }
        }

        // Second pass: place unlabeled args in first unfilled slot.
        int next_positional = 0;
        for (auto& arg : labeled_args) {
          if (arg.label.empty()) {
            while (next_positional < expected_count &&
                   param_filled[next_positional]) {
              ++next_positional;
            }
            if (next_positional < expected_count) {
              ordered_args[next_positional] = arg.value_id;
              param_filled[next_positional] = true;
              ++next_positional;
            }
          }
        }
      } else {
        // No parameters — just use args as-is.
        for (auto& arg : labeled_args) {
          ordered_args.push_back(arg.value_id);
        }
      }

      int actual_count = static_cast<int>(labeled_args.size());
      if (actual_count > expected_count) {
        context.EmitError(node_id, TooManyArguments,
                          expected_count, actual_count);
      } else if (actual_count < expected_count) {
        // Try to fill missing args from default parameter values.
        auto& fn2 = context.functions().Get(fn_decl->function_id);
        bool all_filled = true;
        for (int j = 0; j < expected_count; ++j) {
          if (!ordered_args[j].has_value()) {
            // Check if this param has a default.
            if (j < static_cast<int>(fn2.param_default_nodes.size()) &&
                fn2.param_default_nodes[j].has_value()) {
              ordered_args[j] =
                  HandleExpr(context, fn2.param_default_nodes[j]);
            } else {
              all_filled = false;
            }
          }
        }
        if (!all_filled) {
          context.EmitError(node_id, TooFewArguments,
                            expected_count, actual_count);
        }
      }

    } else if (auto struct_type = callee_inst.TryAs<SemIR::StructType>()) {
      // M69: Check if this is a generic struct template.
      // If CallExprStart has GenericArgumentClause, specialize the struct first.
      {
        auto generic_type_args = ExtractGenericArgs(context, call_start_node);
        if (!generic_type_args.empty()) {
          // Look up the struct's name to find the template.
          auto& orig_scope = context.name_scopes().Get(struct_type->name_scope_id);
          auto tmpl_it = context.generic_type_templates().find(orig_scope.name_id.index);
          if (tmpl_it != context.generic_type_templates().end()) {
            auto specialized_type_id = SpecializeGenericType(
                context, orig_scope.name_id.index, generic_type_args);
            if (specialized_type_id.has_value()) {
              actual_callee_id = specialized_type_id;
              callee_inst = context.insts().Get(specialized_type_id);
              struct_type = callee_inst.TryAs<SemIR::StructType>();
            }
          }
        }
      }

      // --- Struct initialization ---
      // actual_callee_id is the InstId of the StructType instruction.
      // The TypeId for a struct value is derived from the StructType InstId.
      type_id = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(actual_callee_id));

      auto& scope = context.name_scopes().Get(struct_type->name_scope_id);

      // Check for a custom init(...) function in the struct scope.
      // If found, delegate to it rather than using memberwise initialization.
      {
        auto init_ident_id = context.identifiers().Lookup("init");
        if (init_ident_id.has_value()) {
          auto init_name_id = SemIR::NameId::ForIdentifier(init_ident_id);
          auto init_it = scope.names.find(init_name_id.index);
          if (init_it != scope.names.end()) {
            auto init_decl_inst = context.insts().Get(init_it->second);
            if (auto fn_decl = init_decl_inst.TryAs<SemIR::FunctionDecl>()) {
              auto& fn = context.functions().Get(fn_decl->function_id);
              // Match call-site labeled args to the init's params by name.
              llvm::SmallVector<SemIR::InstId> init_args;
              if (fn.call_params_id.has_value() &&
                  fn.call_params_id != SemIR::InstBlockId::Empty) {
                auto param_ids =
                    context.sem_ir().inst_blocks().Get(fn.call_params_id);
                init_args.resize(param_ids.size(), SemIR::InstId::None);
                for (size_t pi = 0; pi < param_ids.size(); ++pi) {
                  auto param_inst = context.insts().Get(param_ids[pi]);
                  if (auto vp = param_inst.TryAs<SemIR::ValueParam>()) {
                    auto p_ident = vp->pretty_name_id.AsIdentifierId();
                    llvm::StringRef pname =
                        p_ident.has_value()
                            ? context.identifiers().Get(p_ident)
                            : llvm::StringRef();
                    for (auto& arg : labeled_args) {
                      if (arg.label == pname) {
                        init_args[pi] = arg.value_id;
                        break;
                      }
                    }
                  }
                }
              } else {
                for (auto& arg : labeled_args) {
                  init_args.push_back(arg.value_id);
                }
              }
              llvm::SmallVector<SemIR::InstId> valid_init_args;
              for (auto a : init_args) {
                if (a.has_value()) valid_init_args.push_back(a);
              }
              auto init_args_block = context.inst_blocks().AddPlaceholder();
              context.inst_blocks().ReplacePlaceholder(
                  init_args_block,
                  llvm::ArrayRef<SemIR::InstId>(valid_init_args));
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::Call{.type_id = type_id,
                              .callee_id = init_it->second,
                              .args_id = init_args_block}));
            }
          }
        }
      }

      // Count struct fields.
      int num_fields = 0;
      for (auto& [name_idx, inst_id] : scope.names) {
        auto field_inst = context.insts().Get(inst_id);
        if (auto field = field_inst.TryAs<SemIR::StructField>()) {
          int idx = field->index.index + 1;
          if (idx > num_fields) num_fields = idx;
        }
      }

      ordered_args.resize(num_fields, SemIR::InstId::None);

      // Match labeled args to fields by name.
      for (auto& arg : labeled_args) {
        if (!arg.label.empty()) {
          auto ident_id = context.identifiers().Lookup(arg.label);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto it = scope.names.find(name_id.index);
            if (it != scope.names.end()) {
              auto field_inst = context.insts().Get(it->second);
              if (auto field = field_inst.TryAs<SemIR::StructField>()) {
                int fi = field->index.index;
                if (fi < num_fields) {
                  ordered_args[fi] = arg.value_id;
                }
              }
            } else {
              context.EmitError(node_id, UnknownStructField,
                                std::string(arg.label));
            }
          } else {
            context.EmitError(node_id, UnknownStructField,
                              std::string(arg.label));
          }
        } else {
          // Positional: put in first unfilled slot.
          for (int j = 0; j < num_fields; ++j) {
            if (!ordered_args[j].has_value()) {
              ordered_args[j] = arg.value_id;
              break;
            }
          }
        }
      }

      // Emit missing-field diagnostics for unfilled slots.
      for (auto& [name_idx, inst_id] : scope.names) {
        auto field_inst = context.insts().Get(inst_id);
        if (auto field = field_inst.TryAs<SemIR::StructField>()) {
          int fi = field->index.index;
          if (fi < num_fields && !ordered_args[fi].has_value()) {
            auto ident_id_opt = field->name_id.AsIdentifierId();
            if (ident_id_opt.has_value()) {
              auto field_text =
                  std::string(context.identifiers().Get(ident_id_opt));
              context.EmitError(node_id, MissingStructField, field_text);
            }
          }
        }
      }

      // Build the args block and emit StructInit.
      llvm::SmallVector<SemIR::InstId> valid_args;
      for (auto arg : ordered_args) {
        valid_args.push_back(arg.has_value() ? arg
                                             : SemIR::InstId::None);
      }
      auto args_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          args_block_id, llvm::ArrayRef<SemIR::InstId>(valid_args));
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::StructInit{.type_id = type_id, .args_id = args_block_id}));

    } else if (auto class_type = callee_inst.TryAs<SemIR::ClassType>()) {
      // --- Class initialization (same as struct memberwise init for now) ---
      type_id = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(actual_callee_id));

      auto& scope = context.name_scopes().Get(class_type->name_scope_id);

      // Check for a custom init(...) in the class scope.
      {
        auto init_ident_id = context.identifiers().Lookup("init");
        if (init_ident_id.has_value()) {
          auto init_name_id = SemIR::NameId::ForIdentifier(init_ident_id);
          auto init_it = scope.names.find(init_name_id.index);
          if (init_it != scope.names.end()) {
            auto init_decl_inst = context.insts().Get(init_it->second);
            if (auto fn_decl = init_decl_inst.TryAs<SemIR::FunctionDecl>()) {
              auto& fn = context.functions().Get(fn_decl->function_id);
              llvm::SmallVector<SemIR::InstId> init_args;
              if (fn.call_params_id.has_value() &&
                  fn.call_params_id != SemIR::InstBlockId::Empty) {
                auto param_ids =
                    context.sem_ir().inst_blocks().Get(fn.call_params_id);
                init_args.resize(param_ids.size(), SemIR::InstId::None);
                for (size_t pi = 0; pi < param_ids.size(); ++pi) {
                  auto param_inst = context.insts().Get(param_ids[pi]);
                  if (auto vp = param_inst.TryAs<SemIR::ValueParam>()) {
                    auto p_ident = vp->pretty_name_id.AsIdentifierId();
                    llvm::StringRef pname = p_ident.has_value()
                        ? context.identifiers().Get(p_ident) : llvm::StringRef();
                    for (auto& arg : labeled_args) {
                      if (arg.label == pname) { init_args[pi] = arg.value_id; break; }
                    }
                  }
                }
              } else {
                for (auto& arg : labeled_args) init_args.push_back(arg.value_id);
              }
              llvm::SmallVector<SemIR::InstId> valid_init_args;
              for (auto a : init_args) { if (a.has_value()) valid_init_args.push_back(a); }
              auto init_args_block = context.inst_blocks().AddPlaceholder();
              context.inst_blocks().ReplacePlaceholder(
                  init_args_block, llvm::ArrayRef<SemIR::InstId>(valid_init_args));
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::Call{.type_id = type_id,
                              .callee_id = init_it->second,
                              .args_id = init_args_block}));
            }
          }
        }
      }

      // M78: Memberwise init for classes — emit AllocClass (heap-allocated).
      int num_fields = 0;
      for (auto& [name_idx, inst_id] : scope.names) {
        auto field_inst = context.insts().Get(inst_id);
        if (auto field = field_inst.TryAs<SemIR::StructField>()) {
          int idx = field->index.index + 1;
          if (idx > num_fields) num_fields = idx;
        }
      }
      ordered_args.resize(num_fields, SemIR::InstId::None);
      for (auto& arg : labeled_args) {
        if (!arg.label.empty()) {
          auto ident_id = context.identifiers().Lookup(arg.label);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto it = scope.names.find(name_id.index);
            if (it != scope.names.end()) {
              auto field_inst = context.insts().Get(it->second);
              if (auto field = field_inst.TryAs<SemIR::StructField>()) {
                int fi = field->index.index;
                if (fi < num_fields) ordered_args[fi] = arg.value_id;
              }
            }
          }
        } else {
          for (int j = 0; j < num_fields; ++j) {
            if (!ordered_args[j].has_value()) { ordered_args[j] = arg.value_id; break; }
          }
        }
      }
      llvm::SmallVector<SemIR::InstId> valid_args;
      for (auto arg : ordered_args) {
        valid_args.push_back(arg.has_value() ? arg : SemIR::InstId::None);
      }
      auto args_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          args_block_id, llvm::ArrayRef<SemIR::InstId>(valid_args));
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::AllocClass{.type_id = type_id, .args_id = args_block_id}));

    } else if (auto closure_expr = callee_inst.TryAs<SemIR::ClosureExpr>()) {
      // --- Closure call: apply the closure as a function ---
      auto& fn = context.functions().Get(closure_expr->function_id);
      if (fn.return_type_inst_id.has_value()) {
        type_id = context.types().GetTypeIdForTypeInstId(
            fn.return_type_inst_id);
      }
      for (auto& arg : labeled_args) {
        ordered_args.push_back(arg.value_id);
      }
    } else if (callee_inst.Is<SemIR::ValueParam>()) {
      // --- Higher-order function call via parameter (e.g., f: (Int) -> Int) ---
      // Function types lower to opaque ptr; use Int as default return type.
      // lower.cpp indirect-call path builds the LLVM FunctionType dynamically.
      type_id = context.GetBuiltinType("Int");
      for (auto& arg : labeled_args) {
        ordered_args.push_back(arg.value_id);
      }
    } else {
      // Callee is not callable — emit error for non-error types.
      for (auto& arg : labeled_args) {
        ordered_args.push_back(arg.value_id);
      }
      auto callee_type = GetInstType(context, callee_id);
      if (!IsErrorType(callee_type)) {
        context.EmitError(node_id, CannotCallNonFunction);
      }
    }
  }

  // Remove None placeholders (unfilled params won't crash SILGen).
  llvm::SmallVector<SemIR::InstId> valid_args;
  for (auto arg : ordered_args) {
    if (arg.has_value()) {
      valid_args.push_back(arg);
    }
  }

  auto args_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      args_block_id, llvm::ArrayRef<SemIR::InstId>(valid_args));

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Call{
          .type_id = type_id, .callee_id = callee_id,
          .args_id = args_block_id}));
}

// M53: DFS scan to collect outer variable captures in a closure body.
// For each IdentifierNameExpr, looks up the name in the CURRENT scope (which
// is still the outer scope when this is called before the closure scope is
// pushed). If the resolved instruction is a VarStorage or ValueBinding (and
// not a closure param name), it's a capture.
// Populates `captures` (deduped) with (outer_inst_id, name_id) pairs.
static auto ScanForCaptures(
    Context& context, Parse::NodeId node_id,
    const llvm::DenseSet<llvm::StringRef>& exclude_names,
    llvm::SmallVector<std::pair<SemIR::InstId, SemIR::NameId>>& captures,
    llvm::DenseSet<int32_t>& seen_ids) -> void {
  auto kind = context.node_kind(node_id);
  if (kind == Parse::NodeKind::IdentifierNameExpr ||
      kind == Parse::NodeKind::IdentifierNameBeforeParams ||
      kind == Parse::NodeKind::IdentifierNameNotBeforeParams) {
    auto text = context.token_text(context.node_token(node_id));
    if (!exclude_names.contains(text)) {
      auto ident_id = context.identifiers().Lookup(text);
      if (ident_id.has_value()) {
        auto name_id = SemIR::NameId::ForIdentifier(ident_id);
        auto value_id = context.LookupName(name_id);
        if (value_id.has_value()) {
          auto inst = context.insts().Get(value_id);
          bool is_outer_var = inst.Is<SemIR::VarStorage>() ||
                              inst.Is<SemIR::ValueBinding>();
          if (is_outer_var && !seen_ids.contains(value_id.index)) {
            seen_ids.insert(value_id.index);
            captures.push_back({value_id, name_id});
          }
        }
      }
    }
  }
  // Recurse into children.
  for (auto child : context.children_source_order(node_id)) {
    ScanForCaptures(context, child, exclude_names, captures, seen_ids);
  }
}

// M46: DFS scan to find the maximum implicit parameter index ($0, $1, etc.).
// Returns the maximum N found (0-based), or -1 if none found.
static auto ScanForDollarIdents(Context& context, Parse::NodeId node_id)
    -> int {
  int max_idx = -1;
  auto kind = context.node_kind(node_id);
  if (kind == Parse::NodeKind::DollarIdentExpr) {
    auto text = context.token_text(context.node_token(node_id));
    // Parse numeric suffix: "$0" → 0, "$1" → 1, etc.
    if (text.size() >= 2 && text[0] == '$') {
      unsigned n = 0;
      auto suffix = text.drop_front(1);
      if (!suffix.getAsInteger(10, n)) {
        max_idx = std::max(max_idx, static_cast<int>(n));
      }
    }
  }
  for (auto child : context.children_source_order(node_id)) {
    int child_max = ScanForDollarIdents(context, child);
    max_idx = std::max(max_idx, child_max);
  }
  return max_idx;
}

// Handles a closure expression `{ param in body }`.
auto HandleClosureExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  // Extract closure signature (parameter name) and body.
  // ClosureParam and ClosureSignature are both leaf nodes (child_count=0) and
  // are direct children of ClosureExpr — ClosureParam is a sibling of
  // ClosureSignature, not a child of it.
  SemIR::NameId param_name_id = SemIR::NameId::None;
  bool has_signature = false;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ClosureSignature) {
      has_signature = true;
    } else if (child_kind == Parse::NodeKind::ClosureParam) {
      // ClosureParam is a direct child of ClosureExpr holding the param token.
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      param_name_id = SemIR::NameId::ForIdentifier(ident_id);
    }
  }

  // M46: Pre-scan body for implicit $N parameters.
  // Only activate when there's no explicit ClosureParam signature.
  int max_dollar_idx = -1;
  if (!has_signature) {
    for (auto child : children) {
      auto child_kind = context.node_kind(child);
      if (child_kind != Parse::NodeKind::ClosureExprStart &&
          child_kind != Parse::NodeKind::ClosureSignature) {
        int idx = ScanForDollarIdents(context, child);
        max_dollar_idx = std::max(max_dollar_idx, idx);
      }
    }
  }

  // Use Int as default parameter/return type for minimal closure support.
  auto int_type = context.GetBuiltinType("Int");

  // M53: Pre-scan body for outer variable captures.
  // This must happen BEFORE the closure scope is pushed so LookupName still
  // resolves names in the outer scope.
  // Collect explicit closure param names to exclude from capture detection.
  llvm::DenseSet<llvm::StringRef> exclude_from_capture;
  if (param_name_id.has_value()) {
    if (auto id = param_name_id.AsIdentifierId(); id.has_value()) {
      exclude_from_capture.insert(context.identifiers().Get(id));
    }
  }
  // Also exclude $N implicit params.
  for (int i = 0; i <= max_dollar_idx; ++i) {
    std::string dname = "$" + std::to_string(i);
    auto did = context.identifiers().Lookup(dname);
    if (did.has_value()) {
      exclude_from_capture.insert(context.identifiers().Get(did));
    }
  }

  llvm::SmallVector<std::pair<SemIR::InstId, SemIR::NameId>> captures;
  llvm::DenseSet<int32_t> seen_capture_ids;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ClosureExprStart ||
        child_kind == Parse::NodeKind::ClosureSignature ||
        child_kind == Parse::NodeKind::ClosureParam) {
      continue;
    }
    ScanForCaptures(context, child, exclude_from_capture, captures,
                    seen_capture_ids);
  }

  // Create a synthetic function for the closure body.
  SemIR::Function closure_fn;
  // Anonymous closure name. Use a static vector so StringRefs into it stay
  // valid for the lifetime of the identifier store (i.e., this compilation).
  {
    static int closure_counter = 0;
    static llvm::SmallVector<std::string>* closure_name_storage =
        new llvm::SmallVector<std::string>();
    closure_name_storage->push_back(
        "__closure_" + std::to_string(closure_counter++));
    auto ident_id =
        context.identifiers().Add(closure_name_storage->back());
    closure_fn.name_id = SemIR::NameId::ForIdentifier(ident_id);
  }
  closure_fn.parent_scope_id = context.CurrentScopeId();
  closure_fn.return_type_inst_id = context.types().GetTypeInstId(int_type);

  // M53: Create capture ValueParams as leading parameters (before explicit params).
  llvm::SmallVector<SemIR::InstId> all_param_ids;
  llvm::SmallVector<std::pair<SemIR::NameId, SemIR::InstId>> capture_param_pairs;
  {
    static auto* capture_param_names = new llvm::SmallVector<std::string>();
    for (int i = 0; i < static_cast<int>(captures.size()); ++i) {
      auto [outer_id, name_id] = captures[i];
      // Use the original name for the capture param.
      capture_param_names->push_back("__capture_" + std::to_string(i));
      // Get the type of the captured variable.
      auto cap_type = GetInstType(context, outer_id);
      auto cap_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::ValueParam{.type_id = cap_type,
                            .index = SemIR::CallParamIndex(i),
                            .pretty_name_id = name_id}));
      all_param_ids.push_back(cap_param_id);
      capture_param_pairs.push_back({name_id, cap_param_id});
    }
  }

  int capture_offset = static_cast<int>(captures.size());

  // Create explicit closure params (after capture params).
  llvm::SmallVector<SemIR::InstId> explicit_param_ids;
  if (has_signature && param_name_id.has_value()) {
    auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ValueParam{.type_id = int_type,
                          .index = SemIR::CallParamIndex(capture_offset),
                          .pretty_name_id = param_name_id}));
    explicit_param_ids.push_back(param_id);
    all_param_ids.push_back(param_id);
  } else if (max_dollar_idx >= 0) {
    // M46: Synthesize $0, $1, ..., $max_dollar_idx implicit parameters.
    static auto* dollar_param_names = new llvm::SmallVector<std::string>();
    for (int i = 0; i <= max_dollar_idx; ++i) {
      dollar_param_names->push_back("$" + std::to_string(i));
      auto ident_id = context.identifiers().Add(dollar_param_names->back());
      auto dname_id = SemIR::NameId::ForIdentifier(ident_id);
      auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::ValueParam{.type_id = int_type,
                            .index = SemIR::CallParamIndex(capture_offset + i),
                            .pretty_name_id = dname_id}));
      explicit_param_ids.push_back(param_id);
      all_param_ids.push_back(param_id);
    }
  }

  if (!all_param_ids.empty()) {
    auto params_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        params_block_id, llvm::ArrayRef<SemIR::InstId>(all_param_ids));
    closure_fn.call_params_id = params_block_id;
    closure_fn.call_param_patterns_id = params_block_id;
  }

  auto decl_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(decl_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());
  closure_fn.decl_block_id = decl_block_id;

  auto function_id = context.functions().Add(closure_fn);

  // Save outer function state and set up closure function context.
  auto outer_function_id = context.CurrentFunctionId();
  context.SetCurrentFunction(function_id);

  // Push a scope for the closure body.
  auto closure_scope_id = context.name_scopes().Add(
      SemIR::InstId::None, closure_fn.name_id, context.CurrentScopeId());
  context.PushScope(closure_scope_id);

  // M53: Register capture params in closure scope, shadowing outer vars.
  for (auto& [cap_name_id, cap_param_id] : capture_param_pairs) {
    context.AddNameToScope(cap_name_id, cap_param_id);
  }

  // Add explicit parameter to scope.
  if (!explicit_param_ids.empty() && has_signature && param_name_id.has_value()) {
    context.AddNameToScope(param_name_id, explicit_param_ids[0]);
  }
  // M46: Add $N implicit parameters to scope.
  if (max_dollar_idx >= 0 && !has_signature) {
    for (int i = 0; i <= max_dollar_idx &&
                    i < static_cast<int>(explicit_param_ids.size()); ++i) {
      std::string dollar_name = "$" + std::to_string(i);
      auto ident_id = context.identifiers().Lookup(dollar_name);
      if (ident_id.has_value()) {
        auto dname_id = SemIR::NameId::ForIdentifier(ident_id);
        context.AddNameToScope(dname_id, explicit_param_ids[i]);
      }
    }
  }

  // Process closure body.
  auto body_block_id = context.PushInstBlock();
  context.functions().Get(function_id).body_block_ids.push_back(body_block_id);

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ClosureExprStart ||
        child_kind == Parse::NodeKind::ClosureSignature ||
        child_kind == Parse::NodeKind::ClosureParam) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Statement |
                                       Parse::NodeCategory::Decl)) {
      HandleStatement(context, child);
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // Implicit return: treat the expression as the return value.
      auto expr_id = HandleExpr(context, child);
      if (expr_id.has_value()) {
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::ReturnExpr{.expr_id = expr_id,
                              .dest_id = SemIR::DestInstId(SemIR::InstId::None)}));
      }
    }
  }

  context.PopInstBlock();
  context.PopScope();
  context.SetCurrentFunction(outer_function_id);

  // M53: Populate captures_id with the outer variable InstIds.
  llvm::SmallVector<SemIR::InstId> capture_outer_ids;
  for (auto& [outer_id, name_id] : captures) {
    // Emit CaptureRef for each captured outer variable.
    auto cap_type = GetInstType(context, outer_id);
    auto capture_ref_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::CaptureRef{.type_id = cap_type,
                          .captured_inst_id = outer_id}));
    capture_outer_ids.push_back(capture_ref_id);
  }

  auto captures_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      captures_block_id, llvm::ArrayRef<SemIR::InstId>(capture_outer_ids));

  // Emit ClosureExpr instruction.
  auto closure_type_id = context.GetBuiltinType("Int");  // simplified
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ClosureExpr{.type_id = closure_type_id,
                         .function_id = function_id,
                         .captures_id = captures_block_id}));
}

// M47: Helper to get the element count of an array (for `.count` property).
// Chases NameRef → VarStorage (via GetArrayVarInit) → ArrayLiteralInit, or
// NameRef → ValueBinding → ArrayLiteralInit.
// Returns the count, or -1 if the base is not an array.
static auto TryGetArrayCount(Context& context, SemIR::InstId base_id) -> int {
  if (!base_id.has_value()) return -1;
  auto inst = context.insts().Get(base_id);
  SemIR::InstId real_id = base_id;
  if (auto nr = inst.TryAs<SemIR::NameRef>()) {
    real_id = nr->value_id;
    inst = context.insts().Get(real_id);
  }
  SemIR::InstId ali_id = SemIR::InstId::None;
  if (inst.Is<SemIR::VarStorage>()) {
    ali_id = context.GetArrayVarInit(real_id);
  } else if (auto vb = inst.TryAs<SemIR::ValueBinding>()) {
    ali_id = vb->value_id;
  }
  if (ali_id.has_value()) {
    auto ali_inst = context.insts().Get(ali_id);
    if (auto ali = ali_inst.TryAs<SemIR::ArrayLiteralInit>()) {
      return static_cast<int>(
          context.inst_blocks().Get(ali->elements_id).size());
    }
  }
  return -1;
}

// Handles a member access expression (base.member).
auto HandleMemberAccessExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  SemIR::InstId base_id = SemIR::InstId::None;
  llvm::StringRef member_name;
  bool is_optional_chain = false;  // M60: x?.foo
  Parse::NodeId generic_arg_clause_node = Parse::NodeId::None;  // M80

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    // M80: Capture GenericArgumentClause if it appears as a direct child.
    if (child_kind == Parse::NodeKind::GenericArgumentClause) {
      generic_arg_clause_node = child;
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      if (!base_id.has_value()) {
        // M60: Detect optional chaining operator (x?.foo).
        if (child_kind == Parse::NodeKind::PostfixOperatorExpr) {
          auto op_text = context.token_text(context.node_token(child));
          if (op_text == "?") {
            is_optional_chain = true;
            // Evaluate the INNER operand of '?' (not the '?' node itself).
            for (auto pc : context.children_source_order(child)) {
              if (context.node_kind(pc).category().HasAnyOf(
                      Parse::NodeCategory::Expr)) {
                base_id = HandleExpr(context, pc);
                break;
              }
            }
            continue;  // Skip normal HandleExpr for this child.
          }
        }
        base_id = HandleExpr(context, child);
      } else {
        // Second Expr child in MemberAccessExpr is the member name
        // (IdentifierNameExpr — same category as base but is just a name token).
        member_name = context.token_text(context.node_token(child));
      }
    } else if (child_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
               child_kind == Parse::NodeKind::IdentifierNameBeforeParams ||
               child_kind.category().HasAnyOf(
                   Parse::NodeCategory::MemberName)) {
      member_name = context.token_text(context.node_token(child));
    }
  }

  // M80: If GenericSpecExpr set pending generic args, pick them up now.
  if (!generic_arg_clause_node.has_value()) {
    generic_arg_clause_node = context.TakePendingGenericArgClause();
  }

  // M60: Optional chaining desugaring (x?.foo → Optional<FooType>).
  // base_id is Optional<T>; member_name is the member to access.
  // Result type: Optional<MemberType>.
  if (is_optional_chain && base_id.has_value() && !member_name.empty()) {
    // Determine inner type T from Optional<T>.
    auto opt_type = GetInstType(context, base_id);
    SemIR::TypeId inner_type_id = SemIR::ErrorInst::TypeId;
    if (opt_type.has_value() && opt_type.is_concrete()) {
      auto opt_ti = context.types().GetTypeInstId(opt_type);
      if (opt_ti.has_value()) {
        auto opt_inst = context.insts().Get(opt_ti);
        if (auto ot = opt_inst.TryAs<SemIR::OptionalType>()) {
          inner_type_id =
              context.types().GetTypeIdForTypeInstId(ot->inner_type_id);
        }
      }
    }

    // Look up the member in T's scope to get member type and field index.
    SemIR::TypeId member_type_id = SemIR::ErrorInst::TypeId;
    SemIR::ElementIndex field_idx(0);
    if (!IsErrorType(inner_type_id) && inner_type_id.is_concrete()) {
      auto inner_ti = context.types().GetTypeInstId(inner_type_id);
      if (inner_ti.has_value()) {
        auto inner_inst = context.insts().Get(inner_ti);
        SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
        if (auto st = inner_inst.TryAs<SemIR::StructType>())
          scope_id = st->name_scope_id;
        else if (auto ct = inner_inst.TryAs<SemIR::ClassType>())
          scope_id = ct->name_scope_id;
        if (scope_id.has_value()) {
          auto ident_id = context.identifiers().Lookup(member_name);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto& scope = context.name_scopes().Get(scope_id);
            auto it = scope.names.find(name_id.index);
            if (it != scope.names.end()) {
              auto mem_inst = context.insts().Get(it->second);
              if (auto field = mem_inst.TryAs<SemIR::StructField>()) {
                member_type_id = field->type_id;
                field_idx = field->index;
              }
            }
          }
        }
      }
    }

    if (!IsErrorType(member_type_id)) {
      // Build Optional<MemberType>.
      auto opt_member_inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::OptionalType{
              .type_id = SemIR::TypeType::TypeId,
              .inner_type_id = SemIR::TypeInstId::UnsafeMake(
                  context.types().GetTypeInstId(member_type_id))}));
      auto opt_member_type = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(opt_member_inst_id));

      auto bool_type = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));

      // has_value = TupleAccess(base, 0)
      auto has_val_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::TupleAccess{.type_id = bool_type,
                             .tuple_id = base_id,
                             .index = SemIR::ElementIndex(0)}));

      // Alloca for result.
      auto temp_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::VarStorage{.type_id = opt_member_type,
                            .pattern_id = SemIR::AbsoluteInstId(
                                SemIR::InstId::None)}));

      auto then_block_id = context.inst_blocks().AddPlaceholder();
      auto else_block_id = context.inst_blocks().AddPlaceholder();
      auto merge_block_id = context.inst_blocks().AddPlaceholder();

      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::BranchIf{.target_id = SemIR::LabelId(then_block_id),
                           .cond_id = has_val_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(else_block_id)}));

      context.SwitchInstBlock(merge_block_id);
      context.AddBodyBlock(merge_block_id);

      // Then block: extract payload, access member, wrap in OptionalSome.
      {
        context.PushInstBlock();
        auto payload_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::TupleAccess{.type_id = inner_type_id,
                               .tuple_id = base_id,
                               .index = SemIR::ElementIndex(1)}));
        auto mem_val_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FieldAccess{.type_id = member_type_id,
                               .base_id = payload_id,
                               .index = field_idx}));
        auto some_val_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::OptionalSome{.type_id = opt_member_type,
                                .value_id = mem_val_id}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Assign{.lhs_id = temp_id, .rhs_id = some_val_id}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
        auto tmp = context.PopInstBlock();
        auto insts = context.inst_blocks().Get(tmp);
        context.inst_blocks().ReplacePlaceholder(
            then_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
        context.AddBodyBlock(then_block_id);
      }

      // Else block: store OptionalNone.
      {
        context.PushInstBlock();
        auto none_val_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::OptionalNone{.type_id = SemIR::ErrorInst::TypeId}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Assign{.lhs_id = temp_id, .rhs_id = none_val_id}));
        context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
        auto tmp = context.PopInstBlock();
        auto insts = context.inst_blocks().Get(tmp);
        context.inst_blocks().ReplacePlaceholder(
            else_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
        context.AddBodyBlock(else_block_id);
      }

      // Load result from alloca in merge block.
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::NameRef{.type_id = opt_member_type,
                         .name_id = SemIR::NameId::None,
                         .value_id = temp_id}));
    }
    // Fallthrough: member not found in optional's inner type.
  }

  // Check if base is a NameRef pointing to an EnumDecl (e.g. `Color.red`).
  // In this case, treat it as a static enum case access and emit EnumInit.
  if (base_id.has_value() && !member_name.empty()) {
    auto base_inst = context.insts().Get(base_id);
    SemIR::InstId actual_base_id = base_id;
    if (auto name_ref = base_inst.TryAs<SemIR::NameRef>()) {
      actual_base_id = name_ref->value_id;
    }
    if (actual_base_id.has_value()) {
      auto actual_inst = context.insts().Get(actual_base_id);
      if (auto enum_decl = actual_inst.TryAs<SemIR::EnumDecl>()) {
        // M80: If this is a generic enum template with type args, specialize
        // before looking up the case in the scope.
        if (generic_arg_clause_node.has_value()) {
          auto& orig_scope = context.name_scopes().Get(enum_decl->name_scope_id);
          auto tmpl_it = context.generic_type_templates().find(
              orig_scope.name_id.index);
          if (tmpl_it != context.generic_type_templates().end()) {
            llvm::SmallVector<SemIR::TypeId> generic_type_args;
            for (auto cc : context.children_source_order(
                     generic_arg_clause_node)) {
              auto cc_kind = context.node_kind(cc);
              if (cc_kind == Parse::NodeKind::GenericArgumentClauseStart ||
                  cc_kind == Parse::NodeKind::PatternListComma) {
                continue;
              }
              if (cc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
                generic_type_args.push_back(HandleTypeExpr(context, cc));
              }
            }
            if (!generic_type_args.empty()) {
              auto specialized_id = SpecializeGenericType(
                  context, orig_scope.name_id.index, generic_type_args);
              if (specialized_id.has_value()) {
                actual_base_id = specialized_id;
                actual_inst = context.insts().Get(specialized_id);
                enum_decl = actual_inst.TryAs<SemIR::EnumDecl>();
              }
            }
          }
        }
        auto& scope = context.name_scopes().Get(enum_decl->name_scope_id);
        auto ident_id = context.identifiers().Lookup(member_name);
        if (ident_id.has_value()) {
          auto name_id = SemIR::NameId::ForIdentifier(ident_id);
          auto it = scope.names.find(name_id.index);
          if (it != scope.names.end()) {
            auto case_inst = context.insts().Get(it->second);
            if (auto ec = case_inst.TryAs<SemIR::EnumCase>()) {
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::EnumInit{.type_id = ec->type_id,
                                  .case_id = it->second}));
            }
            // Enum case with payload (e.g., `Result.success`): emit a NameRef
            // so HandleCallExpr can see EnumCaseWithPayload as the callee.
            if (auto ecwp = case_inst.TryAs<SemIR::EnumCaseWithPayload>()) {
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::NameRef{.type_id = ecwp->type_id,
                                 .name_id = name_id,
                                 .value_id = it->second}));
            }
          }
        }
        context.EmitError(node_id, InvalidMemberAccess,
                          std::string("enum"), std::string(member_name));
        return context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
      }

      // Also handle static method access and nested type access on a struct/class
      // type name (e.g. `Geometry.square(5)` or `Matrix.Row(a:1,b:2)`).
      auto type_scope_id = SemIR::NameScopeId::None;
      if (auto struct_type = actual_inst.TryAs<SemIR::StructType>())
        type_scope_id = struct_type->name_scope_id;
      else if (auto class_type = actual_inst.TryAs<SemIR::ClassType>())
        type_scope_id = class_type->name_scope_id;
      if (type_scope_id.has_value()) {
        auto& scope = context.name_scopes().Get(type_scope_id);
        auto ident_id = context.identifiers().Lookup(member_name);
        if (ident_id.has_value()) {
          auto name_id = SemIR::NameId::ForIdentifier(ident_id);
          auto it = scope.names.find(name_id.index);
          if (it != scope.names.end()) {
            auto member_inst_id = it->second;
            auto member_inst = context.insts().Get(member_inst_id);
            // Static method on struct/class type.
            if (auto fn_decl = member_inst.TryAs<SemIR::FunctionDecl>()) {
              auto& fn = context.functions().Get(fn_decl->function_id);
              if (fn.is_static) {
                return context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::NameRef{.type_id = SemIR::TypeType::TypeId,
                                   .name_id = name_id,
                                   .value_id = member_inst_id}));
              }
            }
            // M62: Nested type access on a struct/class type name
            // (e.g. `Matrix.Row`, `Outer.Inner`).
            if (member_inst.Is<SemIR::StructType>() ||
                member_inst.Is<SemIR::ClassType>() ||
                member_inst.Is<SemIR::EnumDecl>()) {
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::NameRef{.type_id = SemIR::TypeType::TypeId,
                                 .name_id = name_id,
                                 .value_id = member_inst_id}));
            }
          }
        }
      }
    }
  }

  // M88: Check built-in type scopes for members added via extensions.
  if (base_id.has_value() && !member_name.empty()) {
    auto base_type = GetInstType(context, base_id);
    if (base_type.has_value() && base_type.is_concrete()) {
      auto base_type_inst_id = context.types().GetTypeInstId(base_type);
      if (base_type_inst_id.has_value()) {
        auto builtin_scope_id =
            context.GetBuiltinTypeScope(base_type_inst_id);
        if (builtin_scope_id.has_value()) {
          auto& scope = context.name_scopes().Get(builtin_scope_id);
          auto ident_id = context.identifiers().Lookup(member_name);
          if (ident_id.has_value()) {
            auto name_id = SemIR::NameId::ForIdentifier(ident_id);
            auto it = scope.names.find(name_id.index);
            if (it != scope.names.end()) {
              auto member_inst_id = it->second;
              auto member_inst = context.insts().Get(member_inst_id);
              if (member_inst.Is<SemIR::FunctionDecl>()) {
                return context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::BoundMethod{
                        .type_id = SemIR::TypeType::TypeId,
                        .base_id = base_id,
                        .function_id = member_inst_id}));
              }
            }
          }
        }
      }
    }
  }

  // M47: Intercept `.count` on array values — emit compile-time IntValue.
  if (member_name == "count" && base_id.has_value()) {
    int count = TryGetArrayCount(context, base_id);
    if (count >= 0) {
      auto int_type = context.GetBuiltinType("Int");
      auto int_id = context.ints().Add(static_cast<int64_t>(count));
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntValue{.type_id = int_type, .int_id = int_id}));
    }
  }

  // M49: Intercept `.count` and `.isEmpty` on String values.
  if (base_id.has_value() && !member_name.empty()) {
    auto base_type_check = GetInstType(context, base_id);
    if (IsStringType(context, base_type_check)) {
      auto int_type = context.GetBuiltinType("Int");
      if (member_name == "count") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringLen{.type_id = int_type, .operand_id = base_id}));
      }
      if (member_name == "isEmpty") {
        // Desugar: string_len(s) == 0
        auto len_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringLen{.type_id = int_type, .operand_id = base_id}));
        auto zero_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::IntValue{
                .type_id = int_type,
                .int_id = context.ints().Add(static_cast<int64_t>(0))}));
        auto bool_type = SemIR::TypeId::ForTypeConstant(
            SemIR::ConstantId::ForConcreteConstant(
                SemIR::BoolType::TypeInstId));
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntEq{.type_id = bool_type,
                         .lhs_id = len_id,
                         .rhs_id = zero_id}));
      }
      // M61: String methods.
      auto string_type = context.GetBuiltinType("String");
      if (member_name == "uppercased") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringUppercased{.type_id = string_type,
                                    .str_id = base_id}));
      }
      if (member_name == "lowercased") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringLowercased{.type_id = string_type,
                                    .str_id = base_id}));
      }
      if (member_name == "trimmed") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringTrimmed{.type_id = string_type, .str_id = base_id}));
      }
      // Arg-taking string methods: emit StringMethodRef pending marker.
      if (member_name == "hasPrefix" || member_name == "hasSuffix" ||
          member_name == "contains") {
        auto ident_id = context.identifiers().Add(member_name);
        auto method_name_id = SemIR::NameId::ForIdentifier(ident_id);
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringMethodRef{.type_id = SemIR::ErrorInst::TypeId,
                                   .str_id = base_id,
                                   .method_name_id = method_name_id}));
      }
    }
  }

  // M77: UnsafePointer methods (.deallocate, .allocate).
  if (base_id.has_value() && !member_name.empty()) {
    // Check if base is a pointer-typed VALUE (e.g. ptr.deallocate).
    auto base_type_id_ptr = GetInstType(context, base_id);
    bool is_ptr_value = false;
    if (base_type_id_ptr.has_value() && base_type_id_ptr.is_concrete()) {
      auto ptr_ti = context.types().GetTypeInstId(base_type_id_ptr);
      if (ptr_ti.has_value()) {
        auto ptr_inst = context.insts().Get(ptr_ti);
        if (ptr_inst.Is<SemIR::PointerType>()) {
          is_ptr_value = true;
        }
      }
    }
    // Also check if base is a PointerType TYPE instruction (static call like
    // UnsafeMutablePointer<Int>.allocate(capacity:)).
    bool is_ptr_type = false;
    if (!is_ptr_value) {
      auto base_inst = context.insts().Get(base_id);
      if (base_inst.Is<SemIR::PointerType>()) {
        is_ptr_type = true;
      }
    }
    if (is_ptr_value || is_ptr_type) {
      if (member_name == "deallocate" && is_ptr_value) {
        auto ident_id = context.identifiers().Add("deallocate");
        auto method_name_id = SemIR::NameId::ForIdentifier(ident_id);
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::UnsafePtrMethodRef{.type_id = SemIR::ErrorInst::TypeId,
                                      .ptr_id = base_id,
                                      .method_name_id = method_name_id}));
      }
      if (member_name == "allocate") {
        auto ident_id = context.identifiers().Add("allocate");
        auto method_name_id = SemIR::NameId::ForIdentifier(ident_id);
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::UnsafePtrMethodRef{.type_id = SemIR::ErrorInst::TypeId,
                                      .ptr_id = base_id,
                                      .method_name_id = method_name_id}));
      }
    }
  }

  // M65: Dynamic array methods (.count, .isEmpty, .append).
  if (base_id.has_value() && !member_name.empty()) {
    // Resolve base to DynamicArrayInit through NameRef → VarStorage.
    auto base_inst = context.insts().Get(base_id);
    SemIR::InstId dyn_arr_id = SemIR::InstId::None;
    SemIR::InstId real_base_id = base_id;
    if (auto nr = base_inst.TryAs<SemIR::NameRef>()) {
      real_base_id = nr->value_id;
      base_inst = context.insts().Get(real_base_id);
    }
    if (base_inst.Is<SemIR::VarStorage>()) {
      auto dai = context.GetDynamicArrayVar(real_base_id);
      if (dai.has_value()) {
        dyn_arr_id = dai;
        base_inst = context.insts().Get(dyn_arr_id);
      }
    } else if (base_inst.Is<SemIR::DynamicArrayInit>()) {
      dyn_arr_id = real_base_id;
    }
    if (dyn_arr_id.has_value()) {
      auto int_type = context.GetBuiltinType("Int");
      auto bool_type = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(SemIR::BoolType::TypeInstId));
      if (member_name == "count") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::DynamicArrayCount{.type_id = int_type,
                                     .array_id = dyn_arr_id}));
      }
      if (member_name == "isEmpty") {
        auto cnt_id = context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::DynamicArrayCount{.type_id = int_type,
                                     .array_id = dyn_arr_id}));
        auto zero_id = context.AddInst(SemIR::LocIdAndInst::NoLoc(
            SemIR::IntValue{.type_id = int_type,
                            .int_id = context.ints().Add(0LL)}));
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntEq{.type_id = bool_type,
                         .lhs_id = cnt_id,
                         .rhs_id = zero_id}));
      }
      // .append() — emit a pending marker; HandleCallExpr resolves it.
      if (member_name == "append") {
        auto ident_id = context.identifiers().Add("append");
        auto method_name_id = SemIR::NameId::ForIdentifier(ident_id);
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::DynamicArrayMethodRef{.type_id = SemIR::ErrorInst::TypeId,
                                         .array_id = dyn_arr_id,
                                         .method_name_id = method_name_id}));
      }
    }
  }

  auto base_type_id = GetInstType(context, base_id);
  auto type_id = SemIR::ErrorInst::TypeId;
  SemIR::ElementIndex element_index(0);

  if (IsErrorType(base_type_id)) {
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // For struct/class/enum types, look up the member in the type's scope.
  if (base_type_id.has_value() && base_type_id.is_concrete()) {
    auto type_inst_id = context.types().GetTypeInstId(base_type_id);
    if (type_inst_id.has_value()) {
      auto type_inst = context.insts().Get(type_inst_id);

      SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
      if (auto struct_type = type_inst.TryAs<SemIR::StructType>()) {
        scope_id = struct_type->name_scope_id;
      } else if (auto class_type = type_inst.TryAs<SemIR::ClassType>()) {
        scope_id = class_type->name_scope_id;
      } else if (auto enum_type = type_inst.TryAs<SemIR::EnumDecl>()) {
        scope_id = enum_type->name_scope_id;
      } else if (auto tt = type_inst.TryAs<SemIR::TupleType>()) {
        // Numeric member access on tuple: t.0, t.1, ...
        unsigned idx = 0;
        if (!member_name.empty() && !member_name.getAsInteger(10, idx)) {
          auto elems =
              context.inst_blocks().Get(tt->element_types_id);
          if (idx < elems.size()) {
            auto elem_type = context.types().GetTypeIdForTypeInstId(
                SemIR::TypeInstId::UnsafeMake(elems[idx]));
            return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
                SemIR::TupleAccess{.type_id = elem_type,
                                   .tuple_id = base_id,
                                   .index = SemIR::ElementIndex(idx)}));
          }
        }
      }

      if (scope_id.has_value() && !member_name.empty()) {
        auto ident_id = context.identifiers().Lookup(member_name);
        if (ident_id.has_value()) {
          auto name_id = SemIR::NameId::ForIdentifier(ident_id);
          // Search the type's scope for the member.
          auto& scope = context.name_scopes().Get(scope_id);
          auto found = scope.names.find(name_id.index);
          if (found != scope.names.end()) {
            auto member_inst_id = found->second;
            auto member_inst = context.insts().Get(member_inst_id);

            // Computed property: auto-call the getter with self.
            if (auto cpd =
                    member_inst.TryAs<SemIR::ComputedPropertyDecl>()) {
              // Pass base_id as self (SILGen loads VarStorage from NameRef).
              auto args_block_id = context.inst_blocks().AddPlaceholder();
              context.inst_blocks().ReplacePlaceholder(
                  args_block_id,
                  llvm::ArrayRef<SemIR::InstId>{base_id});
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::Call{.type_id = cpd->type_id,
                              .callee_id = cpd->getter_id,
                              .args_id = args_block_id}));
            }

            // If it's a FunctionDecl, emit BoundMethod for instance methods
            // or NameRef for static methods (no receiver).
            if (auto fn_decl = member_inst.TryAs<SemIR::FunctionDecl>()) {
              auto& fn = context.functions().Get(fn_decl->function_id);
              if (fn.is_static) {
                // Static method: no receiver — emit a plain NameRef so
                // HandleCallExpr sees FunctionDecl directly (no self prepend).
                return context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::NameRef{.type_id = SemIR::TypeType::TypeId,
                                   .name_id = name_id,
                                   .value_id = member_inst_id}));
              }
              // Instance method: emit BoundMethod (receiver + method).
              return context.AddInst(SemIR::LocIdAndInst(
                  SemIR::LocId(node_id),
                  SemIR::BoundMethod{.type_id = SemIR::TypeType::TypeId,
                                     .base_id = base_id,
                                     .function_id = member_inst_id}));
            }
            // If it's a StructField, use its index and type.
            if (auto field = member_inst.TryAs<SemIR::StructField>()) {
              type_id = field->type_id;
              element_index = field->index;
            } else {
              type_id = GetInstType(context, member_inst_id);
            }
          } else {
            context.EmitError(node_id, InvalidMemberAccess,
                              context.GetTypeName(base_type_id),
                              std::string(member_name));
          }
        } else {
          context.EmitError(node_id, InvalidMemberAccess,
                            context.GetTypeName(base_type_id),
                            std::string(member_name));
        }
      }
    }
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::FieldAccess{
          .type_id = type_id, .base_id = base_id, .index = element_index}));
}

// Handles an infix operator expression with type checking.
auto HandleInfixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  SemIR::InstId lhs_id = SemIR::InstId::None;
  SemIR::InstId rhs_id = SemIR::InstId::None;

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

  auto op_text = context.token_text(context.node_token(node_id));
  auto lhs_type = GetInstType(context, lhs_id);
  auto rhs_type = GetInstType(context, rhs_id);
  auto bool_type = context.GetBuiltinType("Bool");

  // Compound assignment operators: x op= y → load x, compute (x op y), store back.
  // These are purely a check-phase desugaring: emit the binary op then Assign.
  if (op_text == "+=" || op_text == "-=" || op_text == "*=" ||
      op_text == "/=" || op_text == "%=") {
    auto lhs_type_id = lhs_type;
    SemIR::InstId result_id = SemIR::InstId::None;

    if (IsFloatType(context, lhs_type_id)) {
      if (op_text == "+=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::FloatAdd{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "-=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::FloatSub{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "*=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::FloatMul{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "/=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::FloatDiv{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    } else {
      // Default: integer arithmetic.
      if (op_text == "+=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::IntAdd{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "-=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::IntSub{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "*=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::IntMul{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "/=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::IntDiv{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      else if (op_text == "%=")
        result_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
            SemIR::IntMod{.type_id = lhs_type_id, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }

    if (result_id.has_value()) {
      // Emit Assign. sil_gen's Assign handler already knows how to look through
      // NameRef → VarStorage to get the store address.
      context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = lhs_id, .rhs_id = result_id}));
    }
    return result_id.has_value() ? result_id : lhs_id;
  }

  // Nil coalescing operator `??`: checked BEFORE the error-type propagation
  // because nil literal has ErrorInst::TypeId (no Optional type assigned).
  // `lhs ?? rhs` desugars to: lhs.hasValue ? lhs! : rhs
  // Both LHS and RHS have already been evaluated (no short-circuit for M23).
  if (op_text == "??") {
    auto result_type = rhs_type;
    if (IsErrorType(result_type)) {
      // Both operands are errors — propagate.
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }

    // If LHS is a nil literal (OptionalNone), always return RHS.
    if (lhs_id.has_value()) {
      auto lhs_inst = context.insts().Get(lhs_id);
      if (lhs_inst.Is<SemIR::OptionalNone>()) {
        return rhs_id;
      }
    }

    // General case: LHS is Optional<T> = {i1, T}.
    // Extract has-value flag (field 0).
    auto has_value_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::TupleAccess{.type_id = bool_type,
                           .tuple_id = lhs_id,
                           .index = SemIR::ElementIndex(0)}));

    // Allocate a temporary VarStorage for the result.
    auto temp_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::VarStorage{.type_id = result_type,
                          .pattern_id = SemIR::AbsoluteInstId(
                              SemIR::InstId::None)}));

    auto then_block_id = context.inst_blocks().AddPlaceholder();
    auto else_block_id = context.inst_blocks().AddPlaceholder();
    auto merge_block_id = context.inst_blocks().AddPlaceholder();

    // Emit conditional branch: has_value → then, else → else_block.
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BranchIf{.target_id = SemIR::LabelId(then_block_id),
                         .cond_id = has_value_id}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(else_block_id)}));

    // Switch emission to merge block.
    context.SwitchInstBlock(merge_block_id);
    context.AddBodyBlock(merge_block_id);

    // Then block: extract payload (field 1), assign to temp, branch merge.
    {
      context.PushInstBlock();
      auto payload_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::TupleAccess{.type_id = result_type,
                             .tuple_id = lhs_id,
                             .index = SemIR::ElementIndex(1)}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = temp_id, .rhs_id = payload_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
      auto tmp = context.PopInstBlock();
      auto insts = context.inst_blocks().Get(tmp);
      context.inst_blocks().ReplacePlaceholder(
          then_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
      context.AddBodyBlock(then_block_id);
    }

    // Else block: assign RHS to temp, branch merge.
    {
      context.PushInstBlock();
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = temp_id, .rhs_id = rhs_id}));
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
      auto tmp = context.PopInstBlock();
      auto insts = context.inst_blocks().Get(tmp);
      context.inst_blocks().ReplacePlaceholder(
          else_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
      context.AddBodyBlock(else_block_id);
    }

    // In merge block: load result from temp (NameRef → SILGen emits Load).
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::NameRef{.type_id = result_type,
                       .name_id = SemIR::NameId::None,
                       .value_id = temp_id}));
  }

  // If either operand is an error, propagate without further checking.
  if (IsErrorType(lhs_type) || IsErrorType(rhs_type)) {
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  bool lhs_is_float = IsFloatType(context, lhs_type);
  bool lhs_is_int = IsIntType(context, lhs_type);
  bool lhs_is_bool = IsBoolType(context, lhs_type);
  bool lhs_is_string = IsStringType(context, lhs_type);

  // Arithmetic operators: +, -, *, /, %
  if (op_text == "+" || op_text == "-" || op_text == "*" || op_text == "/" ||
      op_text == "%") {
    // String concatenation for +.
    if (op_text == "+" && lhs_is_string && IsStringType(context, rhs_type)) {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::StringConcat{
              .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }

    // Check operand types match.
    if (lhs_type != rhs_type) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }

    // Float arithmetic.
    if (lhs_is_float) {
      if (op_text == "+") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatAdd{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "-") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatSub{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "*") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatMul{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "/") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatDiv{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      // Float % is not standard in Swift, emit error.
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }

    // Int arithmetic.
    if (lhs_is_int) {
      if (op_text == "+") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntAdd{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "-") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntSub{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "*") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntMul{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "/") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::IntDiv{
                .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      // %
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntMod{
              .type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }

    // M58: Before reporting "not a numeric type", check for operator overloading.
    {
      if (lhs_type.has_value() && lhs_type.is_concrete()) {
        auto ti_id = context.types().GetTypeInstId(lhs_type);
        if (ti_id.has_value()) {
          auto ti = context.insts().Get(ti_id);
          SemIR::NameScopeId sc_id = SemIR::NameScopeId::None;
          if (auto st = ti.TryAs<SemIR::StructType>()) sc_id = st->name_scope_id;
          else if (auto ct = ti.TryAs<SemIR::ClassType>()) sc_id = ct->name_scope_id;
          if (sc_id.has_value()) {
            auto op_eid = context.identifiers().Lookup(op_text);
            if (op_eid.has_value()) {
              auto op_nid = SemIR::NameId::ForIdentifier(op_eid);
              auto& sc = context.name_scopes().Get(sc_id);
              auto it = sc.names.find(op_nid.index);
              if (it != sc.names.end()) {
                auto op_inst = context.insts().Get(it->second);
                if (auto fnd = op_inst.TryAs<SemIR::FunctionDecl>()) {
                  auto& op_fn = context.functions().Get(fnd->function_id);
                  SemIR::TypeId ret = context.GetBuiltinType("Int");
                  if (op_fn.return_type_inst_id.has_value()) {
                    ret = context.types().GetTypeIdForTypeInstId(
                        op_fn.return_type_inst_id);
                  }
                  llvm::SmallVector<SemIR::InstId> args = {lhs_id, rhs_id};
                  auto ab = context.inst_blocks().AddPlaceholder();
                  context.inst_blocks().ReplacePlaceholder(
                      ab, llvm::ArrayRef<SemIR::InstId>(args));
                  return context.AddInst(SemIR::LocIdAndInst(
                      SemIR::LocId(node_id),
                      SemIR::Call{.type_id = ret, .callee_id = it->second,
                                  .args_id = ab}));
                }
              }
            }
          }
        }
      }
    }

    // Not a numeric type.
    context.EmitError(node_id, InvalidOperandTypes,
                      std::string(op_text),
                      context.GetTypeName(lhs_type),
                      context.GetTypeName(rhs_type));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // Comparison operators: ==, !=, <, >, <=, >=
  if (op_text == "==" || op_text == "!=" || op_text == "<" ||
      op_text == ">" || op_text == "<=" || op_text == ">=") {
    // Check operand types match.
    if (lhs_type != rhs_type) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }

    // Float comparisons.
    if (lhs_is_float) {
      if (op_text == "==") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatEq{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "!=") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatNeq{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "<") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatLess{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == ">") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatGreater{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "<=") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::FloatLessEq{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      // >=
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::FloatGreaterEq{
              .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }

    // M45: String comparisons.
    if (IsStringType(context, lhs_type)) {
      if (op_text == "==") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringEq{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
      if (op_text == "!=") {
        return context.AddInst(SemIR::LocIdAndInst(
            SemIR::LocId(node_id),
            SemIR::StringNeq{
                .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
      }
    }

    // Int comparisons.
    if (op_text == "==") {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntEq{
              .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }
    if (op_text == "!=") {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntNeq{
              .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    }
    if (op_text == "<") {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntLess{
              .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
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
    // >=
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntGreaterEq{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }

  // Boolean operators: &&, ||
  if (op_text == "&&") {
    if (!lhs_is_bool || !IsBoolType(context, rhs_type)) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolAnd{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }
  if (op_text == "||") {
    if (!lhs_is_bool || !IsBoolType(context, rhs_type)) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolOr{
            .type_id = bool_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }

  // Bitwise integer operators: &, |, ^, <<, >>
  if (op_text == "&" || op_text == "|" || op_text == "^" ||
      op_text == "<<" || op_text == ">>") {
    if (!lhs_is_int) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(lhs_type),
                        context.GetTypeName(rhs_type));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }
    if (op_text == "&")
      return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
          SemIR::IntBitwiseAnd{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    if (op_text == "|")
      return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
          SemIR::IntBitwiseOr{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    if (op_text == "^")
      return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
          SemIR::IntBitwiseXor{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    if (op_text == "<<")
      return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
          SemIR::IntShiftLeft{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
    // >>
    return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
        SemIR::IntShiftRight{.type_id = lhs_type, .lhs_id = lhs_id, .rhs_id = rhs_id}));
  }

  // M58: Operator overloading — look up operator as a static method in LHS type.
  {
    auto lhs_type_id = GetInstType(context, lhs_id);
    if (lhs_type_id.has_value() && lhs_type_id.is_concrete()) {
      auto type_inst_id = context.types().GetTypeInstId(lhs_type_id);
      if (type_inst_id.has_value()) {
        auto type_inst = context.insts().Get(type_inst_id);
        SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
        if (auto st = type_inst.TryAs<SemIR::StructType>()) {
          scope_id = st->name_scope_id;
        } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
          scope_id = ct->name_scope_id;
        }
        if (scope_id.has_value()) {
          auto op_ident_id = context.identifiers().Lookup(op_text);
          if (op_ident_id.has_value()) {
            auto op_name_id = SemIR::NameId::ForIdentifier(op_ident_id);
            auto& scope = context.name_scopes().Get(scope_id);
            auto it = scope.names.find(op_name_id.index);
            if (it != scope.names.end()) {
              auto op_inst_id = it->second;
              auto op_inst = context.insts().Get(op_inst_id);
              if (auto fn_decl = op_inst.TryAs<SemIR::FunctionDecl>()) {
                auto& op_fn = context.functions().Get(fn_decl->function_id);
                SemIR::TypeId ret_type = context.GetBuiltinType("Int");
                if (op_fn.return_type_inst_id.has_value()) {
                  ret_type = context.types().GetTypeIdForTypeInstId(
                      op_fn.return_type_inst_id);
                }
                llvm::SmallVector<SemIR::InstId> args = {lhs_id, rhs_id};
                auto args_block_id = context.inst_blocks().AddPlaceholder();
                context.inst_blocks().ReplacePlaceholder(
                    args_block_id, llvm::ArrayRef<SemIR::InstId>(args));
                return context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::Call{.type_id = ret_type,
                                .callee_id = op_inst_id,
                                .args_id = args_block_id}));
              }
            }
          }
        }
      }
    }
  }

  // Unknown operator.
  return lhs_id;
}

// Handles a prefix operator expression.
auto HandlePrefixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

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

  if (IsErrorType(operand_type)) {
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  if (op_text == "-") {
    if (IsFloatType(context, operand_type)) {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::FloatNegate{
              .type_id = operand_type, .operand_id = operand_id}));
    }
    if (IsIntType(context, operand_type)) {
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::IntNegate{
              .type_id = operand_type, .operand_id = operand_id}));
    }
    context.EmitError(node_id, InvalidOperandTypes,
                      std::string(op_text),
                      context.GetTypeName(operand_type),
                      std::string(""));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }
  if (op_text == "!") {
    if (!IsBoolType(context, operand_type)) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(operand_type),
                        std::string(""));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BoolNot{.type_id = operand_type, .operand_id = operand_id}));
  }
  if (op_text == "~") {
    if (!IsIntType(context, operand_type)) {
      context.EmitError(node_id, InvalidOperandTypes,
                        std::string(op_text),
                        context.GetTypeName(operand_type),
                        std::string(""));
      return context.AddInst(SemIR::LocIdAndInst::NoLoc(
          SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
    }
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::IntBitwiseNot{.type_id = operand_type, .operand_id = operand_id}));
  }

  return operand_id;
}

// Handles a postfix operator expression.
// Implements `x!` force-unwrap for Optional<T>: extracts field 1 of {i1, T}.
auto HandlePostfixOperatorExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  SemIR::InstId operand_id = SemIR::InstId::None;
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      operand_id = HandleExpr(context, child);
      break;
    }
  }

  auto op_text = context.token_text(context.node_token(node_id));
  if (op_text == "!" && operand_id.has_value()) {
    auto operand_type = GetInstType(context, operand_id);
    if (operand_type.has_value() && operand_type.is_concrete()) {
      auto ti = context.types().GetTypeInstId(operand_type);
      if (ti.has_value()) {
        auto type_inst = context.insts().Get(ti);
        if (auto ot = type_inst.TryAs<SemIR::OptionalType>()) {
          // Optional<T> is lowered as {i1, T}; payload is field index 1.
          auto inner =
              context.types().GetTypeIdForTypeInstId(ot->inner_type_id);
          return context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
              SemIR::TupleAccess{.type_id = inner,
                                 .tuple_id = operand_id,
                                 .index = SemIR::ElementIndex(1)}));
        }
      }
    }
  }

  return operand_id;  // passthrough for unrecognized postfix ops
}

// Handles an assignment expression.
auto HandleAssignmentExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  // Collect LHS and RHS parse nodes without evaluating yet.
  Parse::NodeId lhs_child = Parse::NodeId::None;
  Parse::NodeId rhs_child = Parse::NodeId::None;
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(
            Parse::NodeCategory::Expr)) {
      if (!lhs_child.has_value()) {
        lhs_child = child;
      } else {
        rhs_child = child;
      }
    }
  }

  SemIR::InstId lhs_id = SemIR::InstId::None;
  SemIR::InstId rhs_id = SemIR::InstId::None;

  // If LHS is a MemberAccessExpr (e.g., `self.x = value` in an init body),
  // emit FieldAddr instead of FieldAccess so the assignment writes through.
  if (lhs_child.has_value() &&
      context.node_kind(lhs_child) == Parse::NodeKind::MemberAccessExpr) {
    Parse::NodeId base_child = Parse::NodeId::None;
    llvm::StringRef member_name;
    for (auto mc : context.children_source_order(lhs_child)) {
      auto mc_kind = context.node_kind(mc);
      if (mc_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        if (!base_child.has_value()) {
          base_child = mc;
        } else {
          member_name = context.token_text(context.node_token(mc));
        }
      } else if (mc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                 mc_kind == Parse::NodeKind::IdentifierNameBeforeParams ||
                 mc_kind.category().HasAnyOf(Parse::NodeCategory::MemberName)) {
        member_name = context.token_text(context.node_token(mc));
      }
    }

    if (base_child.has_value() && !member_name.empty()) {
      // To find the VarStorage for the base (e.g., `self`), look up the name
      // directly in scope WITHOUT calling HandleExpr. Calling HandleExpr would
      // emit a NameRef that sil_gen converts to a Load, causing a spurious
      // "use before initialization" error in init bodies.
      SemIR::InstId var_storage_id = SemIR::InstId::None;
      auto base_kind = context.node_kind(base_child);
      if (base_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
          base_kind == Parse::NodeKind::IdentifierNameBeforeParams ||
          base_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        auto base_text =
            context.token_text(context.node_token(base_child));
        auto base_ident_id = context.identifiers().Lookup(base_text);
        if (base_ident_id.has_value()) {
          auto base_name_id = SemIR::NameId::ForIdentifier(base_ident_id);
          auto looked_up = context.LookupName(base_name_id);
          if (looked_up.has_value()) {
            auto lu_inst = context.insts().Get(looked_up);
            // M56: Also accept InoutParam as base (mutating method self).
            if (lu_inst.Is<SemIR::VarStorage>() ||
                lu_inst.Is<SemIR::InoutParam>()) {
              var_storage_id = looked_up;
            }
          }
        }
      }

      if (var_storage_id.has_value()) {
        auto base_type_id = GetInstType(context, var_storage_id);
        if (base_type_id.has_value() && base_type_id.is_concrete()) {
          auto type_inst_id = context.types().GetTypeInstId(base_type_id);
          if (type_inst_id.has_value()) {
            auto type_inst = context.insts().Get(type_inst_id);
            SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
            if (auto st = type_inst.TryAs<SemIR::StructType>()) {
              scope_id = st->name_scope_id;
            } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
              scope_id = ct->name_scope_id;
            }

            if (scope_id.has_value()) {
              auto ident_id = context.identifiers().Lookup(member_name);
              if (ident_id.has_value()) {
                auto name_id = SemIR::NameId::ForIdentifier(ident_id);
                auto& scope = context.name_scopes().Get(scope_id);
                auto it = scope.names.find(name_id.index);
                if (it != scope.names.end()) {
                  auto field_inst = context.insts().Get(it->second);
                  if (auto field = field_inst.TryAs<SemIR::StructField>()) {
                    lhs_id = context.AddInst(SemIR::LocIdAndInst(
                        SemIR::LocId(node_id),
                        SemIR::FieldAddr{.type_id = field->type_id,
                                         .base_id = var_storage_id,
                                         .index = field->index}));
                  } else if (field_inst.Is<SemIR::ComputedPropertyDecl>()) {
                    // M57: Computed property setter. Look up "propName.set"
                    // in the type scope to find the setter FunctionDecl.
                    static auto* setter_names =
                        new llvm::SmallVector<std::string>();
                    setter_names->push_back(
                        std::string(member_name) + ".set");
                    auto setter_ident_id = context.identifiers().Lookup(
                        setter_names->back());
                    if (setter_ident_id.has_value()) {
                      auto setter_name_id =
                          SemIR::NameId::ForIdentifier(setter_ident_id);
                      auto sit = scope.names.find(setter_name_id.index);
                      if (sit != scope.names.end()) {
                        auto setter_inst_id = sit->second;
                        // Emit AddressOf(self) as first arg (mutating).
                        auto addr_id = context.AddInst(SemIR::LocIdAndInst(
                            SemIR::LocId(node_id),
                            SemIR::AddressOf{
                                .type_id = SemIR::ErrorInst::TypeId,
                                .operand_id = var_storage_id}));
                        // Evaluate RHS.
                        if (rhs_child.has_value()) {
                          rhs_id = HandleExpr(context, rhs_child);
                        }
                        // Emit Call to setter(self_ptr, newValue).
                        llvm::SmallVector<SemIR::InstId> setter_args = {
                            addr_id, rhs_id};
                        auto setter_args_block =
                            context.inst_blocks().AddPlaceholder();
                        context.inst_blocks().ReplacePlaceholder(
                            setter_args_block,
                            llvm::ArrayRef<SemIR::InstId>(setter_args));
                        return context.AddInst(SemIR::LocIdAndInst(
                            SemIR::LocId(node_id),
                            SemIR::Call{
                                .type_id = context.GetBuiltinType("Int"),
                                .callee_id = setter_inst_id,
                                .args_id = setter_args_block}));
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // If LHS is a SubscriptExpr (e.g., `nums[0] = 10`), emit ArrayElementAddr.
  if (!lhs_id.has_value() && lhs_child.has_value() &&
      context.node_kind(lhs_child) == Parse::NodeKind::SubscriptExpr) {
    SemIR::InstId array_id = SemIR::InstId::None;
    SemIR::InstId index_id = SemIR::InstId::None;

    // Parse SubscriptExpr children: SubscriptExprStart (contains base) + index.
    auto subscript_children = context.children_source_order(lhs_child);
    for (auto sc : subscript_children) {
      auto sc_kind = context.node_kind(sc);
      if (sc_kind == Parse::NodeKind::SubscriptExprStart) {
        auto start_children = context.children_source_order(sc);
        for (auto ssc : start_children) {
          if (context.node_kind(ssc).category().HasAnyOf(
                  Parse::NodeCategory::Expr)) {
            array_id = HandleExpr(context, ssc);
            break;
          }
        }
      } else if (sc_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        index_id = HandleExpr(context, sc);
      }
    }

    // M77: Check if base is pointer type — emit UnsafePtrSubscriptAddr.
    if (array_id.has_value() && index_id.has_value()) {
      auto base_type_id = GetInstType(context, array_id);
      if (base_type_id.has_value() && base_type_id.is_concrete()) {
        auto ptr_ti = context.types().GetTypeInstId(base_type_id);
        if (ptr_ti.has_value()) {
          auto ptr_inst = context.insts().Get(ptr_ti);
          if (ptr_inst.Is<SemIR::PointerType>()) {
            auto int_type = context.GetBuiltinType("Int");
            lhs_id = context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::UnsafePtrSubscriptAddr{.type_id = int_type,
                                              .ptr_id = array_id,
                                              .index_id = index_id}));
            // Emit RHS and Assign.
            if (rhs_child.has_value()) {
              rhs_id = HandleExpr(context, rhs_child);
            }
            return context.AddInst(SemIR::LocIdAndInst(
                SemIR::LocId(node_id),
                SemIR::Assign{.lhs_id = lhs_id, .rhs_id = rhs_id}));
          }
        }
      }
    }

    // Determine element type and actual array alloca id.
    // For var arrays: look through NameRef → VarStorage → array_var_init_map_.
    // For let arrays: look through NameRef → ValueBinding → ArrayLiteralInit.
    SemIR::TypeId elem_type_id = context.GetBuiltinType("Int");
    SemIR::InstId actual_array_id = array_id;
    if (array_id.has_value()) {
      auto arr_inst = context.insts().Get(array_id);
      SemIR::InstId real_arr_id = array_id;
      if (auto nr = arr_inst.TryAs<SemIR::NameRef>()) {
        real_arr_id = nr->value_id;
        arr_inst = context.insts().Get(real_arr_id);
      }
      if (arr_inst.Is<SemIR::VarStorage>()) {
        // For var arrays: look up the ArrayLiteralInit via array_var_init_map_.
        auto ali_id = context.GetArrayVarInit(real_arr_id);
        if (ali_id.has_value()) {
          actual_array_id = ali_id;
          arr_inst = context.insts().Get(ali_id);
        }
      } else if (auto vb = arr_inst.TryAs<SemIR::ValueBinding>()) {
        if (vb->value_id.has_value()) {
          actual_array_id = vb->value_id;
          arr_inst = context.insts().Get(actual_array_id);
        }
      }
      if (auto ali = arr_inst.TryAs<SemIR::ArrayLiteralInit>()) {
        elem_type_id = ali->type_id;
      }
    }

    lhs_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ArrayElementAddr{.type_id = elem_type_id,
                                .array_id = actual_array_id,
                                .index_id = index_id}));
  }

  // Fall back to regular expr evaluation for LHS if FieldAddr wasn't emitted.
  if (!lhs_id.has_value() && lhs_child.has_value()) {
    lhs_id = HandleExpr(context, lhs_child);
  }
  if (rhs_child.has_value()) {
    rhs_id = HandleExpr(context, rhs_child);
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Assign{.lhs_id = lhs_id, .rhs_id = rhs_id}));
}

// Handles a ternary expression: `condition ? then_expr : else_expr`.
// Lowered to: VarStorage temp + conditional branch + store in each arm + load in merge.
auto HandleTernaryExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId {
  auto children = context.children_source_order(node_id);
  // TernaryExpr has child_count=3: condition, then_expr, else_expr.
  if (children.size() < 3) return SemIR::InstId::None;

  auto cond_node = children[0];
  auto then_node = children[1];
  auto else_node = children[2];

  // Evaluate condition in current block.
  auto cond_id = HandleExpr(context, cond_node);
  if (!cond_id.has_value()) {
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // Use Int as the result type (covers the common case).
  auto result_type = context.GetBuiltinType("Int");

  // Allocate a temporary VarStorage for the result.
  auto temp_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::VarStorage{.type_id = result_type,
                        .pattern_id = SemIR::AbsoluteInstId(
                            SemIR::InstId::None)}));

  // Create blocks for then, else, and merge.
  auto then_block_id = context.inst_blocks().AddPlaceholder();
  auto else_block_id = context.inst_blocks().AddPlaceholder();
  auto merge_block_id = context.inst_blocks().AddPlaceholder();

  // Emit conditional branch in current block: BranchIf→then, Branch→else.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BranchIf{.target_id = SemIR::LabelId(then_block_id),
                       .cond_id = cond_id}));
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Branch{.target_id = SemIR::LabelId(else_block_id)}));

  // Switch emission to merge block; code after the ternary continues there.
  context.SwitchInstBlock(merge_block_id);
  context.AddBodyBlock(merge_block_id);

  // Build then block: evaluate then_expr, assign to temp, branch to merge.
  {
    context.PushInstBlock();
    auto then_val = HandleExpr(context, then_node);
    if (then_val.has_value()) {
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = temp_id, .rhs_id = then_val}));
    }
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
    auto tmp = context.PopInstBlock();
    auto insts = context.inst_blocks().Get(tmp);
    context.inst_blocks().ReplacePlaceholder(
        then_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
    context.AddBodyBlock(then_block_id);
  }

  // Build else block: evaluate else_expr, assign to temp, branch to merge.
  {
    context.PushInstBlock();
    auto else_val = HandleExpr(context, else_node);
    if (else_val.has_value()) {
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::Assign{.lhs_id = temp_id, .rhs_id = else_val}));
    }
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(merge_block_id)}));
    auto tmp = context.PopInstBlock();
    auto insts = context.inst_blocks().Get(tmp);
    context.inst_blocks().ReplacePlaceholder(
        else_block_id, llvm::ArrayRef<SemIR::InstId>(insts));
    context.AddBodyBlock(else_block_id);
  }

  // In merge block: load from temp (NameRef → SILGen emits Load).
  auto load_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::NameRef{.type_id = result_type,
                     .name_id = SemIR::NameId::None,
                     .value_id = temp_id}));
  return load_id;
}

// Handles a tuple expression `(a, b, ...)`.
auto HandleTupleExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  // Collect element values, skipping structural markers.
  // Note: the parser uses ParenExprStart (not TupleExprStart) as the bracket
  // and PatternListComma (not TupleElement) for separators.
  llvm::SmallVector<SemIR::InstId> elem_ids;
  for (auto child : children) {
    auto ck = context.node_kind(child);
    if (ck == Parse::NodeKind::TupleExprStart ||
        ck == Parse::NodeKind::ParenExprStart ||
        ck == Parse::NodeKind::TupleElement ||
        ck == Parse::NodeKind::PatternListComma) continue;
    if (ck.category().HasAnyOf(Parse::NodeCategory::Expr))
      elem_ids.push_back(HandleExpr(context, child));
  }

  // Build element-type block (each entry is the TypeInstId of the elem type).
  llvm::SmallVector<SemIR::InstId> type_insts;
  for (auto id : elem_ids) {
    if (!id.has_value()) continue;
    auto ti = context.types().GetTypeInstId(context.insts().Get(id).type_id());
    type_insts.push_back(SemIR::InstId(ti.index));
  }
  auto types_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(types_block,
      llvm::ArrayRef<SemIR::InstId>(type_insts));

  // Emit TupleType in no-block (constant type instruction).
  auto tt_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::TupleType{.type_id = SemIR::TypeType::TypeId,
                       .element_types_id = types_block}));
  auto tuple_type_id = SemIR::TypeId::ForTypeConstant(
      SemIR::ConstantId::ForConcreteConstant(tt_id));

  // Emit TupleInit.
  auto elems_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(elems_block,
      llvm::ArrayRef<SemIR::InstId>(elem_ids));
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::TupleInit{.type_id = tuple_type_id, .elements_id = elems_block}));
}

// Handles an array literal expression: [5, 10, 15, 20].
// Children: ArrayExprStart, element exprs, PatternListComma separators.
auto HandleArrayExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);
  llvm::SmallVector<SemIR::InstId> elem_ids;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ArrayExprStart ||
        child_kind == Parse::NodeKind::PatternListComma) {
      continue;
    }
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      elem_ids.push_back(HandleExpr(context, child));
    }
  }

  // M65: Empty array literal → DynamicArrayInit (heap-backed dynamic array).
  if (elem_ids.empty()) {
    auto int_type = context.GetBuiltinType("Int");
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::DynamicArrayInit{.type_id = int_type}));
  }

  // Infer element type from first element.
  SemIR::TypeId elem_type_id = SemIR::ErrorInst::TypeId;
  if (!elem_ids.empty() && elem_ids[0].has_value()) {
    elem_type_id = context.insts().Get(elem_ids[0]).type_id();
  }

  auto block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      block_id, llvm::ArrayRef<SemIR::InstId>(elem_ids));

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ArrayLiteralInit{.type_id = elem_type_id,
                              .elements_id = block_id}));
}

// Handles an array subscript access expression: arr[index].
// SubscriptExprStart has subtree_size >= 2 (contains base expr as child).
// Remaining children of SubscriptExpr: the index expression.
auto HandleSubscriptExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  SemIR::InstId array_id = SemIR::InstId::None;
  SemIR::InstId index_id = SemIR::InstId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::SubscriptExprStart) {
      // SubscriptExprStart contains the array expression as its child.
      auto start_children = context.children_source_order(child);
      for (auto sc : start_children) {
        if (context.node_kind(sc).category().HasAnyOf(
                Parse::NodeCategory::Expr)) {
          array_id = HandleExpr(context, sc);
          break;
        }
      }
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // The index expression (after SubscriptExprStart).
      index_id = HandleExpr(context, child);
    }
  }

  // M77: Check if base is a pointer type — emit UnsafePtrSubscript/Addr.
  if (array_id.has_value() && index_id.has_value()) {
    auto base_type_id = GetInstType(context, array_id);
    if (base_type_id.has_value() && base_type_id.is_concrete()) {
      auto ptr_ti = context.types().GetTypeInstId(base_type_id);
      if (ptr_ti.has_value()) {
        auto ptr_inst = context.insts().Get(ptr_ti);
        if (ptr_inst.Is<SemIR::PointerType>()) {
          auto int_type = context.GetBuiltinType("Int");
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::UnsafePtrSubscript{.type_id = int_type,
                                        .ptr_id = array_id,
                                        .index_id = index_id}));
        }
      }
    }
  }

  // Get element type from the ArrayLiteralInit's type_id.
  // For var arrays: look through NameRef → VarStorage → array_var_init_map_.
  SemIR::TypeId elem_type_id = SemIR::ErrorInst::TypeId;
  SemIR::InstId actual_array_id = array_id;  // Use ALI id directly for sil_gen.
  if (array_id.has_value()) {
    // Look through NameRef → ValueBinding or VarStorage → ArrayLiteralInit.
    auto arr_inst = context.insts().Get(array_id);
    SemIR::InstId real_arr_id = array_id;
    if (auto nr = arr_inst.TryAs<SemIR::NameRef>()) {
      real_arr_id = nr->value_id;
      arr_inst = context.insts().Get(real_arr_id);
    }
    if (arr_inst.Is<SemIR::VarStorage>()) {
      // M65: Check for dynamic array first.
      auto dai_id = context.GetDynamicArrayVar(real_arr_id);
      if (dai_id.has_value()) {
        // Dynamic array element access.
        auto dai_inst = context.insts().Get(dai_id);
        if (auto dai = dai_inst.TryAs<SemIR::DynamicArrayInit>()) {
          elem_type_id = context.GetBuiltinType("Int");  // default Int element
          return context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(node_id),
              SemIR::DynamicArrayAccess{.type_id = elem_type_id,
                                        .array_id = dai_id,
                                        .index_id = index_id}));
        }
      }
      // For var arrays: look up the ArrayLiteralInit via array_var_init_map_.
      auto ali_id = context.GetArrayVarInit(real_arr_id);
      if (ali_id.has_value()) {
        actual_array_id = ali_id;
        arr_inst = context.insts().Get(ali_id);
      }
    } else if (auto vb = arr_inst.TryAs<SemIR::ValueBinding>()) {
      if (vb->value_id.has_value()) {
        actual_array_id = vb->value_id;
        arr_inst = context.insts().Get(actual_array_id);
      }
    }
    if (auto ali = arr_inst.TryAs<SemIR::ArrayLiteralInit>()) {
      elem_type_id = ali->type_id;
    }
  }

  // M42: Check if this is a dictionary subscript (DictInit callee).
  SemIR::InstId actual_dict_id = SemIR::InstId::None;
  if (array_id.has_value()) {
    auto check_inst = context.insts().Get(array_id);
    SemIR::InstId real_id = array_id;
    if (auto nr = check_inst.TryAs<SemIR::NameRef>()) {
      real_id = nr->value_id;
      check_inst = context.insts().Get(real_id);
    }
    if (auto vb = check_inst.TryAs<SemIR::ValueBinding>()) {
      real_id = vb->value_id;
      check_inst = context.insts().Get(real_id);
    }
    if (check_inst.Is<SemIR::DictInit>()) {
      actual_dict_id = real_id;
    }
  }

  if (actual_dict_id.has_value()) {
    // Dictionary subscript: return Optional<ValueType>.
    // M42: infer value type as Int (hardcoded for M42).
    auto value_type_id = context.GetBuiltinType("Int");
    // Build Optional<ValueType> TypeId.
    auto opt_inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::OptionalType{.type_id = SemIR::TypeType::TypeId,
                            .inner_type_id = SemIR::TypeInstId::UnsafeMake(
                                context.types().GetTypeInstId(value_type_id))}));
    auto opt_type_id = SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(opt_inst_id));
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::DictAccess{.type_id = opt_type_id,
                          .dict_id = actual_dict_id,
                          .key_id = index_id}));
  }

  // M64: User-defined subscript — look up "subscript" in struct/class scope.
  if (array_id.has_value()) {
    auto arr_type_id = GetInstType(context, array_id);
    if (arr_type_id.has_value() && arr_type_id.is_concrete()) {
      auto arr_ti = context.types().GetTypeInstId(arr_type_id);
      if (arr_ti.has_value()) {
        auto arr_inst = context.insts().Get(arr_ti);
        SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
        if (auto st = arr_inst.TryAs<SemIR::StructType>())
          scope_id = st->name_scope_id;
        else if (auto ct = arr_inst.TryAs<SemIR::ClassType>())
          scope_id = ct->name_scope_id;
        if (scope_id.has_value()) {
          auto sub_ident_id = context.identifiers().Lookup("subscript");
          if (sub_ident_id.has_value()) {
            auto sub_name_id = SemIR::NameId::ForIdentifier(sub_ident_id);
            auto& scope = context.name_scopes().Get(scope_id);
            auto it = scope.names.find(sub_name_id.index);
            if (it != scope.names.end()) {
              auto fn_decl_inst = context.insts().Get(it->second);
              if (auto fn_decl = fn_decl_inst.TryAs<SemIR::FunctionDecl>()) {
                auto& fn = context.functions().Get(fn_decl->function_id);
                auto ret_type_id =
                    fn.return_type_inst_id.has_value()
                        ? context.types().GetTypeIdForTypeInstId(
                              fn.return_type_inst_id)
                        : SemIR::ErrorInst::TypeId;
                llvm::SmallVector<SemIR::InstId> call_args = {array_id};
                if (index_id.has_value()) call_args.push_back(index_id);
                auto args_block_id = context.inst_blocks().AddPlaceholder();
                context.inst_blocks().ReplacePlaceholder(
                    args_block_id,
                    llvm::ArrayRef<SemIR::InstId>(call_args));
                return context.AddInst(SemIR::LocIdAndInst(
                    SemIR::LocId(node_id),
                    SemIR::Call{.type_id = ret_type_id,
                                .callee_id = it->second,
                                .args_id = args_block_id}));
              }
            }
          }
        }
      }
    }
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ArrayAccess{.type_id = elem_type_id,
                         .array_id = actual_array_id,
                         .index_id = index_id}));
}

// M39: `x is T` — static type check.
auto HandleIsExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId {
  auto lhs_id = context.TakePendingCastExpr();
  SemIR::TypeId target_type = SemIR::ErrorInst::TypeId;

  auto children = context.children_source_order(node_id);
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(Parse::NodeCategory::Type)) {
      target_type = HandleTypeExpr(context, child);
      break;
    }
  }

  bool match = false;
  if (lhs_id.has_value() && target_type != SemIR::ErrorInst::TypeId) {
    auto lhs_type = context.insts().Get(lhs_id).type_id();
    match = (lhs_type == target_type);
  }

  auto bool_type = context.GetBuiltinType("Bool");
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::BoolLiteral{.type_id = bool_type,
                         .value = SemIR::BoolValue::From(match)}));
}

// M39: `x as T` — static identity cast.
auto HandleAsExpr(Context& context, Parse::NodeId /*node_id*/) -> SemIR::InstId {
  auto lhs_id = context.TakePendingCastExpr();
  // Static cast: just return the LHS (types must match in M39).
  return lhs_id.has_value() ? lhs_id : SemIR::InstId::None;
}

// M39: `x as? T` — optional cast (returns Optional<T>).
auto HandleAsQuestionExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto lhs_id = context.TakePendingCastExpr();
  SemIR::TypeId target_type = SemIR::ErrorInst::TypeId;

  auto children = context.children_source_order(node_id);
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(Parse::NodeCategory::Type)) {
      target_type = HandleTypeExpr(context, child);
      break;
    }
  }

  if (!lhs_id.has_value()) return SemIR::InstId::None;

  auto lhs_type = context.insts().Get(lhs_id).type_id();
  bool match = (lhs_type == target_type);

  if (match) {
    // Wrap in OptionalSome.
    auto inner_ti = context.types().GetTypeInstId(target_type);
    auto opt_inst_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::OptionalType{.type_id = SemIR::TypeType::TypeId,
                            .inner_type_id = SemIR::TypeInstId::UnsafeMake(inner_ti)}));
    auto opt_type_id = SemIR::TypeId::ForTypeConstant(
        SemIR::ConstantId::ForConcreteConstant(opt_inst_id));
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::OptionalSome{.type_id = opt_type_id, .value_id = lhs_id}));
  } else {
    // Return OptionalNone.
    return context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::OptionalNone{.type_id = SemIR::ErrorInst::TypeId}));
  }
}

// M39: `x as! T` — forced cast (static identity in M39).
auto HandleAsExclaimExpr(Context& context, Parse::NodeId /*node_id*/)
    -> SemIR::InstId {
  auto lhs_id = context.TakePendingCastExpr();
  // For M39: forced cast is identical to identity cast.
  return lhs_id.has_value() ? lhs_id : SemIR::InstId::None;
}

// M81: `try expr` — evaluate inner expression, check error slot.
// If inside a do-catch, branch to catch block on error.
auto HandleTryExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId {
  auto children = context.children_source_order(node_id);
  SemIR::InstId result_id = SemIR::InstId::None;
  for (auto child : children) {
    if (context.node_kind(child).category().HasAnyOf(Parse::NodeCategory::Expr)) {
      result_id = HandleExpr(context, child);
      break;
    }
  }

  // If there's a catch block target, emit error check + conditional branch.
  auto catch_block = context.CurrentCatchBlock();
  if (catch_block.has_value()) {
    auto bool_type = context.GetBuiltinType("Bool");
    auto error_check_id = context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ErrorCheck{.type_id = bool_type}));

    // Create a continue block for the no-error path.
    auto continue_block_id = context.inst_blocks().AddPlaceholder();

    // BranchIf(error → catch) + Branch(no error → continue).
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::BranchIf{.target_id = SemIR::LabelId(catch_block),
                         .cond_id = error_check_id}));
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Branch{.target_id = SemIR::LabelId(continue_block_id)}));

    // Switch to continue block — subsequent code goes here.
    context.SwitchInstBlock(continue_block_id);
    context.AddBodyBlock(continue_block_id);
  }

  return result_id;
}

// M40: `&varName` — address-of expression.
// Returns an AddressOf instruction pointing to the VarStorage.
auto HandleAddressOfExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::IdentifierNameExpr ||
        child_kind == Parse::NodeKind::SelfExpr) {
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Lookup(text);
      if (!ident_id.has_value()) break;
      auto name_id = SemIR::NameId::ForIdentifier(ident_id);
      auto value_id = context.LookupName(name_id);
      if (!value_id.has_value()) break;
      // Get the inner type from the referenced storage.
      auto ref_inst = context.insts().Get(value_id);
      SemIR::TypeId inner_type = SemIR::ErrorInst::TypeId;
      if (auto vs = ref_inst.TryAs<SemIR::VarStorage>()) {
        inner_type = vs->type_id;
      }
      return context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(node_id),
          SemIR::AddressOf{.type_id = inner_type,
                           .operand_id = value_id}));
    }
  }
  return SemIR::InstId::None;
}

// M46: `$0`, `$1` etc. — implicit closure parameter reference.
// Looks up "$0"/"$1" in the current scope (registered by HandleClosureExpr pre-scan).
auto HandleDollarIdentExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto token = context.node_token(node_id);
  auto text = context.token_text(token);

  auto ident_id = context.identifiers().Lookup(text);
  if (!ident_id.has_value()) {
    context.EmitError(node_id, UndefinedName, std::string(text));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  auto name_id = SemIR::NameId::ForIdentifier(ident_id);
  auto value_id = context.LookupName(name_id);
  if (!value_id.has_value()) {
    context.EmitError(node_id, UndefinedName, std::string(text));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  auto type_id = GetInstType(context, value_id);
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::NameRef{
          .type_id = type_id, .name_id = name_id, .value_id = value_id}));
}

// M42: `["k": v, ...]` — dictionary literal.
auto HandleDictionaryExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  llvm::SmallVector<SemIR::InstId> key_ids;
  llvm::SmallVector<SemIR::InstId> val_ids;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::ArrayExprStart ||
        child_kind == Parse::NodeKind::DictionaryExprStart ||
        child_kind == Parse::NodeKind::PatternListComma) {
      continue;
    }
    if (child_kind == Parse::NodeKind::DictionaryExprEntry) {
      // DictionaryExprEntry has exactly 2 Expr children: key and value.
      auto entry_children = context.children_source_order(child);
      SemIR::InstId entry_key = SemIR::InstId::None;
      SemIR::InstId entry_val = SemIR::InstId::None;
      int expr_count = 0;
      for (auto ec : entry_children) {
        if (context.node_kind(ec).category().HasAnyOf(Parse::NodeCategory::Expr)) {
          if (expr_count == 0) entry_key = HandleExpr(context, ec);
          else if (expr_count == 1) entry_val = HandleExpr(context, ec);
          ++expr_count;
        }
      }
      if (entry_key.has_value()) key_ids.push_back(entry_key);
      if (entry_val.has_value()) val_ids.push_back(entry_val);
    }
  }

  // Infer key/value types.
  auto string_type_id = context.GetBuiltinType("String");
  SemIR::TypeId val_type_id = SemIR::ErrorInst::TypeId;
  if (!val_ids.empty() && val_ids[0].has_value()) {
    val_type_id = context.insts().Get(val_ids[0]).type_id();
  }

  // Build interleaved entries block: [k0, v0, k1, v1, ...].
  llvm::SmallVector<SemIR::InstId> entries;
  for (size_t i = 0; i < key_ids.size(); ++i) {
    entries.push_back(key_ids[i]);
    if (i < val_ids.size()) entries.push_back(val_ids[i]);
  }
  auto entries_block = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(
      entries_block, llvm::ArrayRef<SemIR::InstId>(entries));

  (void)val_type_id;  // M42: type inferred at lower time from values.
  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::DictInit{.type_id = string_type_id,
                      .entries_id = entries_block}));
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
  // M80: GenericSpecExpr wraps `Identifier<Args>` — evaluate the base
  // identifier and store the generic argument clause for use by the
  // parent MemberAccessExpr.
  if (kind == Parse::NodeKind::GenericSpecExpr) {
    auto spec_children = context.children_source_order(node_id);
    SemIR::InstId base_result = SemIR::InstId::None;
    for (auto child : spec_children) {
      auto ck = context.node_kind(child);
      if (ck == Parse::NodeKind::GenericArgumentClause) {
        context.SetPendingGenericArgClause(child);
      } else if (ck.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        base_result = HandleExpr(context, child);
      }
    }
    return base_result;
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
  if (kind == Parse::NodeKind::ClosureExpr) {
    return HandleClosureExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::TernaryExpr) {
    return HandleTernaryExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::TupleExpr) {
    return HandleTupleExpr(context, node_id);
  }

  if (kind == Parse::NodeKind::ArrayExpr) {
    return HandleArrayExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::SubscriptExpr) {
    return HandleSubscriptExpr(context, node_id);
  }

  // M39: Cast expressions (LHS already stored via SetPendingCastExpr).
  if (kind == Parse::NodeKind::IsExpr) {
    return HandleIsExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::AsExpr) {
    return HandleAsExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::AsQuestionExpr) {
    return HandleAsQuestionExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::AsExclaimExpr) {
    return HandleAsExclaimExpr(context, node_id);
  }

  // M41: try expression — pass through.
  if (kind == Parse::NodeKind::TryExpr ||
      kind == Parse::NodeKind::TryExclaimExpr ||
      kind == Parse::NodeKind::TryQuestionExpr) {
    return HandleTryExpr(context, node_id);
  }

  // M40: &varName — address-of.
  if (kind == Parse::NodeKind::AddressOfExpr) {
    return HandleAddressOfExpr(context, node_id);
  }

  // M42: Dictionary literal.
  if (kind == Parse::NodeKind::DictionaryExpr) {
    return HandleDictionaryExpr(context, node_id);
  }

  // M46: Implicit closure parameters $0, $1, etc.
  if (kind == Parse::NodeKind::DollarIdentExpr) {
    return HandleDollarIdentExpr(context, node_id);
  }

  // For expression statements, unwrap.
  if (kind == Parse::NodeKind::ExprStatement) {
    auto children = context.children_source_order(node_id);
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
