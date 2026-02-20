// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_expr.h"

#include "toolchain/check/handle_stmt.h"
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

  // Labeled arguments: (label_text, value_inst_id).
  struct LabeledArg {
    llvm::StringRef label;
    SemIR::InstId value_id;
  };
  llvm::SmallVector<LabeledArg> labeled_args;
  llvm::StringRef pending_label;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::CallExprStart) {
      // CallExprStart has the callee as its child.
      auto start_children = context.children_source_order(child);
      for (auto start_child : start_children) {
        if (context.node_kind(start_child)
                .category()
                .HasAnyOf(Parse::NodeCategory::Expr)) {
          callee_id = HandleExpr(context, start_child);
          break;
        }
      }
    } else if (child_kind == Parse::NodeKind::ArgumentLabel) {
      // Record the label for the next argument.
      pending_label = context.token_text(context.node_token(child));
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      // Argument expression — associate with pending label.
      labeled_args.push_back({pending_label, HandleExpr(context, child)});
      pending_label = llvm::StringRef();
    }
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

    if (auto fn_decl = callee_inst.TryAs<SemIR::FunctionDecl>()) {
      // --- Function call with label matching ---
      auto& fn = context.functions().Get(fn_decl->function_id);
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
        context.EmitError(node_id, TooFewArguments,
                          expected_count, actual_count);
      }

    } else if (auto struct_type = callee_inst.TryAs<SemIR::StructType>()) {
      // --- Struct initialization ---
      // actual_callee_id is the InstId of the StructType instruction.
      // The TypeId for a struct value is derived from the StructType InstId.
      type_id = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(actual_callee_id));

      auto& scope = context.name_scopes().Get(struct_type->name_scope_id);

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

  // Use Int as default parameter/return type for minimal closure support.
  auto int_type = context.GetBuiltinType("Int");

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

  // Create a parameter if signature is present.
  llvm::SmallVector<SemIR::InstId> param_ids;
  if (has_signature && param_name_id.has_value()) {
    auto param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::ValueParam{.type_id = int_type,
                          .index = SemIR::CallParamIndex(0),
                          .pretty_name_id = param_name_id}));
    param_ids.push_back(param_id);

    auto params_block_id = context.inst_blocks().AddPlaceholder();
    context.inst_blocks().ReplacePlaceholder(
        params_block_id, llvm::ArrayRef<SemIR::InstId>(param_ids));
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

  // Add parameter to scope.
  if (!param_ids.empty() && param_name_id.has_value()) {
    context.AddNameToScope(param_name_id, param_ids[0]);
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

  // Emit ClosureExpr instruction.
  auto closure_type_id = context.GetBuiltinType("Int");  // simplified
  auto captures_block_id = context.inst_blocks().AddPlaceholder();
  context.inst_blocks().ReplacePlaceholder(captures_block_id,
                                           llvm::ArrayRef<SemIR::InstId>());

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::ClosureExpr{.type_id = closure_type_id,
                         .function_id = function_id,
                         .captures_id = captures_block_id}));
}

// Handles a member access expression (base.member).
auto HandleMemberAccessExpr(Context& context, Parse::NodeId node_id)
    -> SemIR::InstId {
  auto children = context.children_source_order(node_id);

  SemIR::InstId base_id = SemIR::InstId::None;
  llvm::StringRef member_name;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      if (!base_id.has_value()) {
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
  if (kind == Parse::NodeKind::ClosureExpr) {
    return HandleClosureExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::TernaryExpr) {
    return HandleTernaryExpr(context, node_id);
  }
  if (kind == Parse::NodeKind::TupleExpr) {
    return HandleTupleExpr(context, node_id);
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
