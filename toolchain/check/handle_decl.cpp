// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_decl.h"

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

auto HandleLetDecl(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // Pre-scan: detect tuple pattern destructuring `let (a, b) = expr`.
  // Children in source order: LetIntroducer, IdentifierPattern*, TuplePattern,
  // <RHS expr>, LetInitializer.
  bool is_tuple_pattern = false;
  llvm::SmallVector<Parse::NodeId> tuple_elem_names;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::TuplePattern) {
      is_tuple_pattern = true;
      break;
    } else if (child_kind == Parse::NodeKind::IdentifierPattern) {
      tuple_elem_names.push_back(child);
    }
  }

  if (is_tuple_pattern) {
    // Evaluate the RHS expression (the Expr child after TuplePattern).
    SemIR::InstId init_id = SemIR::InstId::None;
    bool past_tuple_pattern = false;
    for (auto child : children) {
      auto child_kind = context.node_kind(child);
      if (child_kind == Parse::NodeKind::TuplePattern) {
        past_tuple_pattern = true;
        continue;
      }
      if (!past_tuple_pattern) continue;
      if (child_kind == Parse::NodeKind::LetInitializer) continue;
      if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        init_id = HandleExpr(context, child);
        break;
      }
    }

    // For each element name at index i, emit TupleAccess + ValueBinding.
    for (size_t i = 0; i < tuple_elem_names.size(); ++i) {
      auto elem_name_node = tuple_elem_names[i];
      auto token = context.node_token(elem_name_node);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      auto elem_name_id = SemIR::NameId::ForIdentifier(ident_id);

      // Infer element type from the TupleInit's element at index i,
      // or from the TupleType's element_types_id if RHS is a function call.
      SemIR::TypeId elem_type_id = SemIR::ErrorInst::TypeId;
      if (init_id.has_value()) {
        auto init_inst = context.insts().Get(init_id);
        if (auto ti = init_inst.TryAs<SemIR::TupleInit>()) {
          auto elem_ids = context.inst_blocks().Get(ti->elements_id);
          if (i < elem_ids.size()) {
            elem_type_id = context.insts().Get(elem_ids[i]).type_id();
          }
        } else {
          // RHS is a function call returning a tuple; get element type from
          // the TupleType instruction's element_types_id block.
          auto type_ti = context.types().GetTypeInstId(init_inst.type_id());
          if (type_ti.has_value()) {
            if (auto tt =
                    context.insts().Get(type_ti).TryAs<SemIR::TupleType>()) {
              auto elems = context.inst_blocks().Get(tt->element_types_id);
              if (i < elems.size()) {
                elem_type_id = context.types().GetTypeIdForTypeInstId(
                    SemIR::TypeInstId::UnsafeMake(elems[i]));
              }
            }
          }
        }
      }

      // Emit TupleAccess for element i.
      auto elem_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::TupleAccess{.type_id = elem_type_id,
                             .tuple_id = init_id,
                             .index = SemIR::ElementIndex(
                                 static_cast<int32_t>(i))}));

      // Create ValueBinding and register name.
      auto entity_name_id = context.entity_names().Add(
          {.name_id = elem_name_id,
           .parent_scope_id = context.CurrentScopeId()});

      auto pattern_block_id = context.inst_blocks().AddPlaceholder();
      auto pattern_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::ValueBindingPattern{.type_id = elem_type_id,
                                     .entity_name_id = entity_name_id}));
      context.inst_blocks().ReplacePlaceholder(
          pattern_block_id, llvm::ArrayRef<SemIR::InstId>({pattern_id}));

      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::NameBindingDecl{.pattern_block_id = pattern_block_id}));

      auto binding_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::ValueBinding{.type_id = elem_type_id,
                              .entity_name_id = entity_name_id,
                              .value_id = elem_id}));

      context.AddNameToScope(elem_name_id, binding_id);
    }
    return;
  }

  SemIR::NameId name_id = SemIR::NameId::None;
  SemIR::TypeId type_id = SemIR::TypeId::None;
  SemIR::InstId init_id = SemIR::InstId::None;
  Parse::NodeId name_node_id = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::LetIntroducer) {
      continue;
    }

    if (child_kind == Parse::NodeKind::LetBindingPattern) {
      // LetBindingPattern has name and type.
      auto pattern_children = context.children_source_order(child);
      for (auto pc : pattern_children) {
        auto pc_kind = context.node_kind(pc);
        if (pc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams) {
          name_node_id = pc;
          auto token = context.node_token(pc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          name_id = SemIR::NameId::ForIdentifier(ident_id);
        } else if (pc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
          type_id = HandleTypeExpr(context, pc);
        } else if (pc_kind == Parse::NodeKind::TypeAnnotation) {
          type_id = HandleTypeExpr(context, pc);
        }
      }
    } else if (child_kind == Parse::NodeKind::IdentifierPattern) {
      // Simple identifier pattern without type.
      name_node_id = child;
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      name_id = SemIR::NameId::ForIdentifier(ident_id);
    } else if (child_kind == Parse::NodeKind::LetInitializer) {
      continue;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      init_id = HandleExpr(context, child);
    } else if (child_kind == Parse::NodeKind::TypeAnnotation) {
      type_id = HandleTypeExpr(context, child);
    }
  }

  // Infer type from initializer if needed.
  if (!type_id.has_value() && init_id.has_value()) {
    type_id = context.insts().Get(init_id).type_id();
  }
  if (!type_id.has_value()) {
    type_id = SemIR::ErrorInst::TypeId;
  }

  // Coerce non-optional initializer to Optional<T> when the declared type is
  // optional (e.g. `let x: Int? = 5` wraps 5 in OptionalSome).
  if (type_id.has_value() && init_id.has_value()) {
    auto ti = context.types().GetTypeInstId(type_id);
    if (ti.has_value()) {
      if (context.insts().Get(ti).Is<SemIR::OptionalType>()) {
        auto init_inst = context.insts().Get(init_id);
        // nil (OptionalNone) and already-wrapped values must not be re-wrapped.
        bool already_optional =
            init_inst.Is<SemIR::OptionalNone>() ||
            init_inst.Is<SemIR::OptionalSome>();
        if (!already_optional) {
          auto init_ti = context.types().GetTypeInstId(init_inst.type_id());
          already_optional = init_ti.has_value() &&
              context.insts().Get(init_ti).Is<SemIR::OptionalType>();
        }
        if (!already_optional) {
          init_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
              SemIR::OptionalSome{.type_id = type_id, .value_id = init_id}));
        }
      }
    }
  }

  // Create entity name.
  auto entity_name_id = context.entity_names().Add(
      {.name_id = name_id, .parent_scope_id = context.CurrentScopeId()});

  // Create pattern block.
  auto pattern_block_id = context.inst_blocks().AddPlaceholder();

  // Emit ValueBindingPattern.
  auto pattern_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::ValueBindingPattern{
          .type_id = type_id, .entity_name_id = entity_name_id}));

  context.inst_blocks().ReplacePlaceholder(
      pattern_block_id, llvm::ArrayRef<SemIR::InstId>({pattern_id}));

  // Emit NameBindingDecl.
  context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::NameBindingDecl{.pattern_block_id = pattern_block_id}));

  // Emit ValueBinding.
  auto binding_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::ValueBinding{.type_id = type_id,
                          .entity_name_id = entity_name_id,
                          .value_id = init_id.has_value()
                                          ? init_id
                                          : SemIR::InstId::None}));

  // Register name in scope.
  if (name_id.has_value()) {
    context.AddNameToScope(name_id, binding_id);
  }
}

auto HandleVariableDecl(Context& context, Parse::NodeId node_id) -> void {
  auto children = context.children_source_order(node_id);

  // Pre-scan: detect tuple pattern destructuring `var (x, y) = expr`.
  // Children in source order: VariableIntroducer, IdentifierPattern*, TuplePattern,
  // <RHS expr>, VariableInitializer.
  bool is_tuple_pattern = false;
  llvm::SmallVector<Parse::NodeId> tuple_elem_names;
  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::TuplePattern) {
      is_tuple_pattern = true;
      break;
    } else if (child_kind == Parse::NodeKind::IdentifierPattern) {
      tuple_elem_names.push_back(child);
    }
  }

  if (is_tuple_pattern) {
    // Evaluate the RHS expression (the Expr child after TuplePattern).
    SemIR::InstId init_id = SemIR::InstId::None;
    bool past_tuple_pattern = false;
    for (auto child : children) {
      auto child_kind = context.node_kind(child);
      if (child_kind == Parse::NodeKind::TuplePattern) {
        past_tuple_pattern = true;
        continue;
      }
      if (!past_tuple_pattern) continue;
      if (child_kind == Parse::NodeKind::VariableInitializer) continue;
      if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
        init_id = HandleExpr(context, child);
        break;
      }
    }

    // For each element name at index i, emit TupleAccess + VarStorage + Assign.
    for (size_t i = 0; i < tuple_elem_names.size(); ++i) {
      auto elem_name_node = tuple_elem_names[i];
      auto token = context.node_token(elem_name_node);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      auto elem_name_id = SemIR::NameId::ForIdentifier(ident_id);

      // Infer element type from the TupleInit's element at index i,
      // or from the TupleType's element_types_id if RHS is a function call.
      SemIR::TypeId elem_type_id = SemIR::ErrorInst::TypeId;
      if (init_id.has_value()) {
        auto init_inst = context.insts().Get(init_id);
        if (auto ti = init_inst.TryAs<SemIR::TupleInit>()) {
          auto elem_ids = context.inst_blocks().Get(ti->elements_id);
          if (i < elem_ids.size()) {
            elem_type_id = context.insts().Get(elem_ids[i]).type_id();
          }
        } else {
          // RHS is a function call returning a tuple; get element type from
          // the TupleType instruction's element_types_id block.
          auto type_ti = context.types().GetTypeInstId(init_inst.type_id());
          if (type_ti.has_value()) {
            if (auto tt =
                    context.insts().Get(type_ti).TryAs<SemIR::TupleType>()) {
              auto elems = context.inst_blocks().Get(tt->element_types_id);
              if (i < elems.size()) {
                elem_type_id = context.types().GetTypeIdForTypeInstId(
                    SemIR::TypeInstId::UnsafeMake(elems[i]));
              }
            }
          }
        }
      }

      // Emit TupleAccess for element i.
      auto elem_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::TupleAccess{.type_id = elem_type_id,
                             .tuple_id = init_id,
                             .index = SemIR::ElementIndex(
                                 static_cast<int32_t>(i))}));

      // Emit VarStorage (mutable binding).
      auto var_id = context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::VarStorage{.type_id = elem_type_id,
                            .pattern_id = SemIR::AbsoluteInstId(
                                SemIR::InstId::None)}));

      // Assign element value to var storage.
      context.AddInst(SemIR::LocIdAndInst(
          SemIR::LocId(elem_name_node),
          SemIR::Assign{.lhs_id = var_id, .rhs_id = elem_id}));

      // Register name in scope as VarStorage (mutable).
      context.AddNameToScope(elem_name_id, var_id);
    }
    return;
  }

  SemIR::NameId name_id = SemIR::NameId::None;
  SemIR::TypeId type_id = SemIR::TypeId::None;
  SemIR::InstId init_id = SemIR::InstId::None;
  Parse::NodeId name_node_id = Parse::NodeId::None;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);

    if (child_kind == Parse::NodeKind::VariableIntroducer) {
      continue;
    }

    if (child_kind == Parse::NodeKind::IdentifierPattern) {
      // Direct identifier pattern: var i = n (no type annotation).
      name_node_id = child;
      auto token = context.node_token(child);
      auto text = context.token_text(token);
      auto ident_id = context.identifiers().Add(text);
      name_id = SemIR::NameId::ForIdentifier(ident_id);
    } else if (child_kind == Parse::NodeKind::VariablePattern) {
      // VariablePattern wraps an inner pattern: var i: Int = n
      auto vp_children = context.children_source_order(child);
      for (auto vpc : vp_children) {
        auto vpc_kind = context.node_kind(vpc);
        if (vpc_kind == Parse::NodeKind::VarBindingPattern) {
          auto bp_children = context.children_source_order(vpc);
          for (auto bpc : bp_children) {
            auto bpc_kind = context.node_kind(bpc);
            if (bpc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams) {
              name_node_id = bpc;
              auto token = context.node_token(bpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (bpc_kind.category().HasAnyOf(
                           Parse::NodeCategory::Type)) {
              type_id = HandleTypeExpr(context, bpc);
            } else if (bpc_kind == Parse::NodeKind::TypeAnnotation) {
              type_id = HandleTypeExpr(context, bpc);
            }
          }
        } else if (vpc_kind == Parse::NodeKind::IdentifierPattern) {
          name_node_id = vpc;
          auto token = context.node_token(vpc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          name_id = SemIR::NameId::ForIdentifier(ident_id);
        }
      }
    } else if (child_kind == Parse::NodeKind::TypeAnnotation) {
      // Direct type annotation for `var name: Type = expr` when IdentifierPattern
      // is used directly (not wrapped in VariablePattern).
      type_id = HandleTypeExpr(context, child);
    } else if (child_kind == Parse::NodeKind::VariableInitializer) {
      continue;
    } else if (child_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
      init_id = HandleExpr(context, child);
    }
  }

  // Infer type from initializer if needed.
  if (!type_id.has_value() && init_id.has_value()) {
    type_id = context.insts().Get(init_id).type_id();
  }
  if (!type_id.has_value()) {
    type_id = SemIR::ErrorInst::TypeId;
  }

  // Coerce non-optional initializer to Optional<T> when the declared type is
  // optional (e.g. `var x: Int? = 5` wraps 5 in OptionalSome).
  if (type_id.has_value() && init_id.has_value()) {
    auto ti = context.types().GetTypeInstId(type_id);
    if (ti.has_value()) {
      if (context.insts().Get(ti).Is<SemIR::OptionalType>()) {
        auto init_inst = context.insts().Get(init_id);
        // nil (OptionalNone) and already-wrapped values must not be re-wrapped.
        bool already_optional =
            init_inst.Is<SemIR::OptionalNone>() ||
            init_inst.Is<SemIR::OptionalSome>();
        if (!already_optional) {
          auto init_ti = context.types().GetTypeInstId(init_inst.type_id());
          already_optional = init_ti.has_value() &&
              context.insts().Get(init_ti).Is<SemIR::OptionalType>();
        }
        if (!already_optional) {
          init_id = context.AddInst(SemIR::LocIdAndInst(SemIR::LocId(node_id),
              SemIR::OptionalSome{.type_id = type_id, .value_id = init_id}));
        }
      }
    }
  }

  // Create entity name for the pattern.
  auto entity_name_id = context.entity_names().Add(
      {.name_id = name_id, .parent_scope_id = context.CurrentScopeId()});
  (void)entity_name_id;  // Used for name tracking; VarStorage references the
                          // pattern indirectly.

  // Emit VarStorage.
  auto var_id = context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(name_node_id.has_value() ? name_node_id : node_id),
      SemIR::VarStorage{.type_id = type_id,
                        .pattern_id = SemIR::AbsoluteInstId(
                            SemIR::InstId::None)}));

  // Emit Assign if we have an initializer.
  if (init_id.has_value()) {
    context.AddInst(SemIR::LocIdAndInst(
        SemIR::LocId(node_id),
        SemIR::Assign{.lhs_id = var_id, .rhs_id = init_id}));

    // Track array var: if initializer is ArrayLiteralInit, record the mapping
    // so subscript access can find the real array alloca.
    if (init_id.has_value()) {
      auto init_inst = context.insts().Get(init_id);
      if (init_inst.Is<SemIR::ArrayLiteralInit>()) {
        context.SetArrayVarInit(var_id, init_id);
      }
    }
  }

  // Register name in scope.
  if (name_id.has_value()) {
    context.AddNameToScope(name_id, var_id);
  }
}

}  // namespace TinySwift::Check
