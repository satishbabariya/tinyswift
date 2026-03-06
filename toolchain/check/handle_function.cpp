// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_function.h"

#include <deque>

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_stmt.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {

// M75/M76: Extracted attribute info from an @extern("C") or @cdecl("name") node.
struct AttributeInfo {
  bool is_extern_c = false;
  bool is_cdecl = false;
  std::string cdecl_name;
};

// Extracts attribute information from an Attribute parse node.
auto ExtractAttributeInfo(Context& context, Parse::NodeId attr_node_id)
    -> AttributeInfo {
  AttributeInfo info;
  if (!attr_node_id.has_value()) return info;

  auto children = context.children_source_order(attr_node_id);
  // Attribute children: AttributeStart, then args (string literals, identifiers).
  // The attribute name is the token after '@' on the AttributeStart node.
  llvm::StringRef attr_name;
  llvm::StringRef attr_arg;

  for (auto child : children) {
    auto kind = context.node_kind(child);
    if (kind == Parse::NodeKind::AttributeStart) {
      // The AttributeStart token is '@', but the actual name is the next
      // identifier. Let's read the token text — it might be the attribute name.
      auto token = context.node_token(child);
      attr_name = context.token_text(token);
      // If token is '@', the name is the next token. Check children for names.
      continue;
    }
    // Child tokens after AttributeStart — could be the attribute name or args.
    auto token = context.node_token(child);
    auto text = context.token_text(token);
    if (attr_name.empty() || attr_name == "@") {
      attr_name = text;
    } else {
      // This is an argument. Remove surrounding quotes if present.
      if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        attr_arg = text.substr(1, text.size() - 2);
      } else {
        attr_arg = text;
      }
    }
  }

  // If attr_name is still "@", try reading the attribute from children tokens.
  // The pattern is: @extern("C") → AttributeStart(@), Identifier(extern),
  // StringLiteral("C").

  if (attr_name == "extern") {
    if (attr_arg == "C" || attr_arg.empty()) {
      info.is_extern_c = true;
    }
  } else if (attr_name == "cdecl") {
    info.is_cdecl = true;
    info.cdecl_name = attr_arg.str();
  }
  return info;
}

// M66-M68: Extract generic parameter names and optional constraints from a
// FunctionDefinitionStart (or StructDefinitionStart, etc.) node.
auto ExtractGenericParams(Context& context, Parse::NodeId sig_node_id)
    -> llvm::SmallVector<SemIR::Function::GenericParamInfo> {
  llvm::SmallVector<SemIR::Function::GenericParamInfo> result;
  auto children = context.children_source_order(sig_node_id);

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::GenericParameterClause) {
      // Iterate children of GenericParameterClause in source order.
      auto clause_children = context.children_source_order(child);
      SemIR::Function::GenericParamInfo pending_info;
      for (auto cc : clause_children) {
        auto cc_kind = context.node_kind(cc);
        if (cc_kind == Parse::NodeKind::GenericParameterClauseStart ||
            cc_kind == Parse::NodeKind::PatternListComma) {
          continue;
        }
        if (cc_kind == Parse::NodeKind::GenericParameter) {
          // If we have a pending info from a previous param, push it.
          if (pending_info.name_id.has_value()) {
            result.push_back(pending_info);
          }
          auto token = context.node_token(cc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_info = SemIR::Function::GenericParamInfo{};
          pending_info.name_id = SemIR::NameId::ForIdentifier(ident_id);
        } else if (cc_kind == Parse::NodeKind::IdentifierType) {
          // Constraint type after `: Protocol` — attach to pending param.
          auto token = context.node_token(cc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_info.constraint_name_id = SemIR::NameId::ForIdentifier(ident_id);
        }
      }
      // Push last param.
      if (pending_info.name_id.has_value()) {
        result.push_back(pending_info);
      }
      break;  // Only one GenericParameterClause per definition.
    }
  }
  return result;
}

// Processes the function signature (name, params, return type) from a
// FunctionDefinitionStart or FunctionDecl node.
struct FunctionSignature {
  SemIR::NameId name_id = SemIR::NameId::None;
  llvm::SmallVector<std::pair<SemIR::NameId, SemIR::TypeId>> params;
  // Default value parse node for each param (Parse::NodeId::None if no default).
  llvm::SmallVector<Parse::NodeId> param_defaults;
  // Whether each param is inout (M40).
  llvm::SmallVector<bool> param_is_inout;
  SemIR::TypeId return_type_id = SemIR::TypeId::None;
  Parse::NodeId name_node_id = Parse::NodeId::None;
  // Whether the function is declared with `throws` (M41).
  bool is_throwing = false;
};

auto ExtractFunctionSignature(Context& context, Parse::NodeId sig_node_id)
    -> FunctionSignature {
  FunctionSignature sig;
  auto children = context.children_source_order(sig_node_id);

  // Type nodes that appear as DIRECT children of FunctionDefinitionStart
  // between ExplicitParamList and ReturnType are element types for a tuple
  // return type like `-> (Int, Int)`. In the parse tree, `(Int, Int)` stores
  // TupleType as the only child of ReturnType (a leaf marker), while the
  // element types Int, Int are siblings of ReturnType within FunctionDefinitionStart.
  llvm::SmallVector<Parse::NodeId> pre_return_type_nodes;

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
      continue;
    }

    if (child_kind == Parse::NodeKind::ExplicitParamList) {
      // Process parameters in source order.
      // NOTE: For `_ x: inout Int`, the parse tree structure is:
      //   ExplicitParamList
      //     ExplicitParamListStart
      //     ExternalParamName("_")   <- sibling
      //     IdentifierPattern("x")   <- sibling of FunctionParam (NOT child)
      //     FunctionParam (children: InoutMarker, TypeAnnotation)
      // For `_ x: Int`, the structure is:
      //   ExplicitParamList
      //     ExplicitParamListStart
      //     ExternalParamName("_")   <- sibling
      //     FunctionParam (children: IdentifierPattern, TypeAnnotation)
      // So we track a pending sibling IdentifierPattern for the next FunctionParam.
      auto param_children = context.children_source_order(child);
      SemIR::NameId pending_sibling_name = SemIR::NameId::None;

      for (auto pc : param_children) {
        auto pc_kind = context.node_kind(pc);

        // Track sibling IdentifierPattern as the pending internal param name.
        if (pc_kind == Parse::NodeKind::IdentifierPattern) {
          auto token = context.node_token(pc);
          auto text = context.token_text(token);
          auto ident_id = context.identifiers().Add(text);
          pending_sibling_name = SemIR::NameId::ForIdentifier(ident_id);
          continue;
        }

        if (pc_kind == Parse::NodeKind::FunctionParam) {
          // FunctionParam has children: InoutMarker? + TypeAnnotation,
          // and optional IdentifierPattern as a CHILD (for non-inout params)
          // or as a SIBLING (captured in pending_sibling_name, for inout params).
          SemIR::NameId param_name_id = pending_sibling_name;
          SemIR::TypeId param_type_id = SemIR::ErrorInst::TypeId;
          Parse::NodeId default_node = Parse::NodeId::None;
          bool is_inout = false;  // M40: track inout modifier
          pending_sibling_name = SemIR::NameId::None;  // consume pending name

          auto fp_children = context.children_source_order(pc);
          for (auto fpc : fp_children) {
            auto fpc_kind = context.node_kind(fpc);
            if (fpc_kind == Parse::NodeKind::InoutMarker) {
              // M40: InoutMarker is a direct child of FunctionParam for
              // `_ x: inout T` (because FunctionParam.child_count=2 takes
              // InoutMarker + TypeAnnotation, leaving IdentifierPattern as sibling).
              is_inout = true;
            } else if (fpc_kind == Parse::NodeKind::ExternalParamName) {
              // External label (`_` or `label`) — skip for body name.
            } else if (fpc_kind == Parse::NodeKind::IdentifierNameNotBeforeParams ||
                       fpc_kind == Parse::NodeKind::IdentifierNameBeforeParams) {
              // Internal parameter name (single-name case: `name: Type`).
              auto token = context.node_token(fpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (fpc_kind == Parse::NodeKind::IdentifierPattern) {
              // Internal name as direct child of FunctionParam
              // (for non-inout params like `_ x: Int`).
              auto token = context.node_token(fpc);
              auto text = context.token_text(token);
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            } else if (fpc_kind == Parse::NodeKind::TypeAnnotation ||
                       fpc_kind.category().HasAnyOf(
                           Parse::NodeCategory::Type)) {
              // For generic types like `UnsafeMutablePointer<Int>`, the
              // IdentifierType appears as a direct child of FunctionParam
              // (outside TypeAnnotation's subtree), so it's processed first
              // and sets param_type_id correctly. The subsequent TypeAnnotation
              // may fail to resolve (its children are just GenericArgumentClause).
              // Don't overwrite a valid type with an error.
              auto resolved = HandleTypeExpr(context, fpc);
              if (resolved != SemIR::ErrorInst::TypeId ||
                  param_type_id == SemIR::ErrorInst::TypeId) {
                param_type_id = resolved;
              }
            } else if (fpc_kind.category().HasAnyOf(Parse::NodeCategory::Expr)) {
              // Trailing expr child is the default value expression.
              default_node = fpc;
            }
          }

          // Fallback: use the FunctionParam's own token as the param name.
          if (!param_name_id.has_value()) {
            auto token = context.node_token(pc);
            auto text = context.token_text(token);
            if (text != "_") {  // skip wildcard
              auto ident_id = context.identifiers().Add(text);
              param_name_id = SemIR::NameId::ForIdentifier(ident_id);
            }
          }

          sig.params.push_back({param_name_id, param_type_id});
          sig.param_defaults.push_back(default_node);
          sig.param_is_inout.push_back(is_inout);
        }
      }
      continue;
    }

    if (child_kind == Parse::NodeKind::ReturnType) {
      auto rt_children = context.children_source_order(child);

      // Check if the ReturnType's child is a TupleType node (tuple return).
      // If so, `pre_return_type_nodes` holds the element types.
      bool has_tuple_child = false;
      for (auto rtc : rt_children) {
        if (context.node_kind(rtc) == Parse::NodeKind::TupleType) {
          has_tuple_child = true;
          break;
        }
      }

      if (has_tuple_child && !pre_return_type_nodes.empty()) {
        // Build a TupleType with properly populated element_types_id.
        // Each pre_return_type_node is an element type (e.g., IdentifierType(Int)).
        llvm::SmallVector<SemIR::InstId> elem_type_insts;
        for (auto tn : pre_return_type_nodes) {
          auto et_id = HandleTypeExpr(context, tn);
          auto et_inst_id = context.types().GetTypeInstId(et_id);
          if (et_inst_id.has_value()) {
            // TypeInstId IS-A InstId (inherits), so store directly.
            elem_type_insts.push_back(et_inst_id);
          }
        }
        auto block = context.inst_blocks().AddPlaceholder();
        context.inst_blocks().ReplacePlaceholder(
            block, llvm::ArrayRef<SemIR::InstId>(elem_type_insts));
        auto tt_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(child),
            SemIR::TupleType{.type_id = SemIR::TypeType::TypeId,
                             .element_types_id = block}));
        sig.return_type_id = SemIR::TypeId::ForTypeConstant(
            SemIR::ConstantId::ForConcreteConstant(tt_id));
      } else {
        // Non-tuple return type: use the first Type child of ReturnType.
        for (auto rtc : rt_children) {
          if (context.node_kind(rtc).category().HasAnyOf(
                  Parse::NodeCategory::Type)) {
            sig.return_type_id = HandleTypeExpr(context, rtc);
            break;
          }
        }
        // Fallback: for generic return types like `UnsafeMutablePointer<Int>`,
        // the IdentifierType is a sibling of ReturnType (outside its subtree),
        // collected in pre_return_type_nodes. Use it if ReturnType had no
        // Type children of its own.
        if (!sig.return_type_id.has_value() && !pre_return_type_nodes.empty()) {
          sig.return_type_id =
              HandleTypeExpr(context, pre_return_type_nodes.back());
        }
      }
      pre_return_type_nodes.clear();
      continue;
    }

    // Collect type nodes that appear between ExplicitParamList and ReturnType.
    // These are element types for a tuple return type `-> (Int, Int)`.
    if (child_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
      pre_return_type_nodes.push_back(child);
    }
  }

  return sig;
}

}  // namespace

auto HandleFunctionDefinition(Context& context, Parse::NodeId node_id,
                              bool is_static_hint,
                              bool is_mutating_hint) -> void {
  // M52: Save outer function context so nested function definitions don't
  // clobber the outer function's state.
  auto outer_function_id = context.CurrentFunctionId();
  // Save and clear deferred blocks for the outer function (M51 × M52 interaction).
  // The nested function gets its own empty defer stack; the outer function's
  // deferred blocks are restored after the nested function is processed.
  auto outer_deferred_blocks = context.GetDeferredBlocks();
  context.ClearDeferredBlocks();

  auto children = context.children_source_order(node_id);

  // First child should be FunctionDefinitionStart.
  // StaticModifier / MutatingModifier are SIBLINGS of FunctionDefinition in the
  // parent CodeBlock — detected by HandleTypeMembers and passed via hints.
  Parse::NodeId sig_node_id = Parse::NodeId::None;
  Parse::NodeId body_code_block = Parse::NodeId::None;
  bool is_static = is_static_hint;
  bool is_mutating = is_mutating_hint;

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::FunctionDefinitionStart) {
      sig_node_id = child;
    } else if (child_kind == Parse::NodeKind::CodeBlock) {
      body_code_block = child;
    }
  }

  if (!sig_node_id.has_value()) {
    return;
  }

  auto sig = ExtractFunctionSignature(context, sig_node_id);

  // M66-M68: Detect generic parameters. If this is a generic template definition
  // (not a specialization re-entry), store template metadata and skip body.
  auto generic_params = ExtractGenericParams(context, sig_node_id);
  if (!generic_params.empty() && !context.is_specializing()) {
    // Create a stub Function with is_generic_template = true.
    SemIR::Function fn;
    fn.name_id = sig.name_id;
    fn.parent_scope_id = context.CurrentScopeId();
    fn.is_generic_template = true;
    fn.generic_params = generic_params;
    fn.template_node_id = node_id;
    fn.template_sig_node_id = sig_node_id;
    fn.source_check_ir_id = context.current_file_id();
    fn.is_static = is_static_hint;
    fn.is_mutating = is_mutating_hint;
    fn.is_throwing = sig.is_throwing;
    fn.param_default_nodes = sig.param_defaults;

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

    // Restore outer function context (M52).
    context.SetCurrentFunction(outer_function_id);
    for (auto block : outer_deferred_blocks) {
      context.PushDeferredBlock(block);
    }
    return;  // Skip body processing for generic templates.
  }

  // Check if we're inside a type (struct/class). If so, either synthesize
  // `self` (instance method) or skip it (static method), and mangle the name.
  auto enclosing_type_id = context.CurrentTypeInstId();
  int param_index_offset = 0;
  SemIR::InstId self_param_id = SemIR::InstId::None;
  SemIR::NameId self_name_id = SemIR::NameId::None;
  SemIR::NameId original_name_id = sig.name_id;

  if (enclosing_type_id.has_value()) {
    // Determine the type name for mangling.
    auto type_inst = context.insts().Get(enclosing_type_id);
    llvm::StringRef type_name;
    if (auto st = type_inst.TryAs<SemIR::StructType>()) {
      auto& scope = context.name_scopes().Get(st->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) {
        type_name = context.identifiers().Get(ident_opt);
      }
    } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
      auto& scope = context.name_scopes().Get(ct->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) {
        type_name = context.identifiers().Get(ident_opt);
      }
    } else if (auto ed = type_inst.TryAs<SemIR::EnumDecl>()) {
      // M89: Enum methods need name mangling too.
      auto& scope = context.name_scopes().Get(ed->name_scope_id);
      auto ident_opt = scope.name_id.AsIdentifierId();
      if (ident_opt.has_value()) {
        type_name = context.identifiers().Get(ident_opt);
      }
    } else if (type_inst.Is<SemIR::IntLiteralType>()) {
      type_name = "Int";
    } else if (auto int_type = type_inst.TryAs<SemIR::IntType>()) {
      // IntType covers Bool (Int1), Int (Int64), UInt, and fixed-width types.
      int bit_width = 64;
      if (int_type->bit_width_id.has_value()) {
        auto width_inst = context.insts().Get(int_type->bit_width_id);
        if (auto iv = width_inst.TryAs<SemIR::IntValue>()) {
          auto ap = context.sem_ir().ints().Get(iv->int_id);
          bit_width = static_cast<int>(ap.getSExtValue());
        }
      }
      if (bit_width == 1 && int_type->int_kind.is_signed()) {
        type_name = "Bool";
      } else if (bit_width == 64 && int_type->int_kind.is_signed()) {
        type_name = "Int";
      } else if (bit_width == 64 && !int_type->int_kind.is_signed()) {
        type_name = "UInt";
      }
    } else if (type_inst.Is<SemIR::StringType>()) {
      type_name = "String";
    } else if (type_inst.Is<SemIR::FloatType>()) {
      type_name = "Float";
    } else if (type_inst.Is<SemIR::DoubleType>()) {
      type_name = "Double";
    }

    if (!type_name.empty() && sig.name_id.has_value() &&
        sig.name_id.AsIdentifierId().has_value()) {
      auto method_name = context.identifiers().Get(sig.name_id.AsIdentifierId());
      // Build "TypeName.methodName" using a deque so that push_back never
      // invalidates references to earlier elements (SmallVector can reallocate).
      static std::deque<std::string>* method_name_storage =
          new std::deque<std::string>();
      method_name_storage->push_back(
          std::string(type_name) + "." + std::string(method_name));
      auto mangled_ident_id =
          context.identifiers().Add(method_name_storage->back());
      sig.name_id = SemIR::NameId::ForIdentifier(mangled_ident_id);
    }

    // For instance methods only: synthesize a `self` parameter at index 0.
    // Static methods do NOT get a self parameter.
    // M56: Mutating methods use InoutParam for self (pointer to struct).
    // M54: All class instance methods are implicitly mutating.
    if (!is_static) {
      auto self_type_id = SemIR::TypeId::ForTypeConstant(
          SemIR::ConstantId::ForConcreteConstant(enclosing_type_id));
      auto self_ident_id = context.identifiers().Add("self");
      self_name_id = SemIR::NameId::ForIdentifier(self_ident_id);

      // M54: Class instance methods are implicitly mutating (self = InoutParam).
      bool self_is_inout = is_mutating;
      if (!self_is_inout) {
        auto type_inst2 = context.insts().Get(enclosing_type_id);
        if (type_inst2.Is<SemIR::ClassType>()) {
          self_is_inout = true;
          is_mutating = true;  // fn.is_mutating set later from is_mutating
        }
      }

      if (self_is_inout) {
        self_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                       : node_id),
            SemIR::InoutParam{.type_id = self_type_id,
                              .index = SemIR::CallParamIndex(0),
                              .name_id = self_name_id}));
      } else {
        self_param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                       : node_id),
            SemIR::ValueParam{.type_id = self_type_id,
                              .index = SemIR::CallParamIndex(0),
                              .pretty_name_id = self_name_id}));
      }
      param_index_offset = 1;
    }
  }

  // M52: If this is a nested function (inside another function, not inside a type),
  // mangle the name for LLVM symbol uniqueness to avoid conflicts between nested
  // functions with the same name in different outer functions.
  if (outer_function_id.has_value() && !enclosing_type_id.has_value() &&
      sig.name_id.has_value() && sig.name_id.AsIdentifierId().has_value()) {
    static int nested_counter = 0;
    static auto* nested_name_storage = new std::deque<std::string>();
    auto orig = context.identifiers().Get(sig.name_id.AsIdentifierId());
    nested_name_storage->push_back(
        "__nested_" + std::to_string(nested_counter++) + "_" + orig.str());
    sig.name_id = SemIR::NameId::ForIdentifier(
        context.identifiers().Add(nested_name_storage->back()));
    // original_name_id stays unmangled and is registered in scope for
    // call-site lookup.
  }

  // M98: Detect Generator<T> return type → mark as generator.
  bool is_generator = false;
  SemIR::TypeId generator_element_type_id = SemIR::TypeId::None;
  if (sig.return_type_id.has_value() && sig.return_type_id.is_concrete()) {
    auto rt_inst_id = context.types().GetTypeInstId(sig.return_type_id);
    if (rt_inst_id.has_value()) {
      auto rt_inst = context.insts().Get(rt_inst_id);
      if (auto gen_type = rt_inst.TryAs<SemIR::GeneratorType>()) {
        is_generator = true;
        generator_element_type_id =
            context.types().GetTypeIdForTypeInstId(gen_type->element_type_id);
      }
    }
  }

  // M100: Detect `async` modifier in function signature.
  bool is_async = false;
  {
    auto sig_children = context.children_source_order(sig_node_id);
    for (auto c : sig_children) {
      if (context.node_kind(c) == Parse::NodeKind::AsyncModifier) {
        is_async = true;
        break;
      }
    }
  }

  // M74: Consume pending access level.
  auto access_level = context.TakePendingAccessLevel();

  // M75/M76: Consume pending attribute.
  auto attr_node = context.TakePendingAttribute();
  auto attr_info = ExtractAttributeInfo(context, attr_node);

  // M112: Consume pending comptime hint.
  bool is_comptime = context.TakePendingComptimeHint();

  // Create a new function in the function store.
  SemIR::Function fn;
  fn.name_id = sig.name_id;  // mangled name if inside a type/nested, else original
  fn.parent_scope_id = context.CurrentScopeId();
  fn.is_static = is_static;
  fn.is_mutating = is_mutating;
  fn.is_throwing = sig.is_throwing;
  fn.is_generator = is_generator;
  fn.generator_element_type_id = generator_element_type_id;
  fn.is_async = is_async;
  fn.is_comptime = is_comptime;
  fn.param_default_nodes = sig.param_defaults;
  fn.access_level = access_level;
  fn.is_extern_c = attr_info.is_extern_c;
  fn.is_cdecl = attr_info.is_cdecl;
  fn.cdecl_name = attr_info.cdecl_name;
  if (attr_info.is_extern_c && sig.name_id.has_value()) {
    auto ident_id = sig.name_id.AsIdentifierId();
    if (ident_id.has_value()) {
      fn.extern_name = context.identifiers().Get(ident_id).str();
    }
  }

  // Create parameter patterns and params.
  llvm::SmallVector<SemIR::InstId> param_pattern_ids;
  llvm::SmallVector<SemIR::InstId> param_ids;

  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    auto param_index =
        SemIR::CallParamIndex(static_cast<int32_t>(i) + param_index_offset);

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

    // Create ValueParam or InoutParam (M40).
    SemIR::InstId param_id = SemIR::InstId::None;
    bool is_inout = (i < sig.param_is_inout.size()) && sig.param_is_inout[i];
    if (is_inout) {
      param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                      : node_id),
          SemIR::InoutParam{.type_id = param_type_id,
                            .index = param_index,
                            .name_id = param_name_id}));
    } else {
      param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
          SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                      : node_id),
          SemIR::ValueParam{.type_id = param_type_id,
                            .index = param_index,
                            .pretty_name_id = param_name_id}));
    }
    param_ids.push_back(param_id);
  }

  // Store param patterns and params as blocks. Prepend `self` if in a method.
  {
    llvm::SmallVector<SemIR::InstId> all_param_ids;
    if (self_param_id.has_value()) {
      all_param_ids.push_back(self_param_id);
    }
    all_param_ids.append(param_ids.begin(), param_ids.end());

    if (!all_param_ids.empty()) {
      auto params_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          params_block_id, llvm::ArrayRef<SemIR::InstId>(all_param_ids));
      fn.call_params_id = params_block_id;
    }

    if (!param_pattern_ids.empty()) {
      auto patterns_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          patterns_block_id,
          llvm::ArrayRef<SemIR::InstId>(param_pattern_ids));
      fn.call_param_patterns_id = patterns_block_id;
    }
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

  // Register function name in scope. Use the ORIGINAL (unmangled) name so
  // that `p.method` lookup in HandleMemberAccessExpr works correctly.
  if (original_name_id.has_value()) {
    context.AddNameToScope(original_name_id, fn_decl_id);
  }

  // M74: Record access level for the declaration.
  context.SetAccessLevel(fn_decl_id, access_level, context.CurrentScopeId());

  // M112: Register comptime function for interpreter use.
  if (is_comptime) {
    context.comptime_evaluator().RegisterComptimeFunction(function_id, node_id);
  }

  // Track the current function for body block registration.
  context.SetCurrentFunction(function_id);

  // Now process the body.
  // Push a new scope for the function body.
  auto body_scope_id = context.name_scopes().Add(
      fn_decl_id, sig.name_id, context.CurrentScopeId());
  context.PushScope(body_scope_id);

  // If this is a method, add `self` to the function body scope.
  if (self_param_id.has_value() && self_name_id.has_value()) {
    context.AddNameToScope(self_name_id, self_param_id);
  }

  // Add regular parameters to the function's scope.
  for (size_t i = 0; i < sig.params.size(); ++i) {
    auto [param_name_id, param_type_id] = sig.params[i];
    context.AddNameToScope(param_name_id, param_ids[i]);
  }

  // Push a new inst block for the function body.
  // Add the main body block to body_block_ids FIRST so it's always bb0 (entry).
  auto body_block_id = context.PushInstBlock();
  context.functions().Get(function_id).body_block_ids.push_back(body_block_id);

  // M78: Push ARC cleanup scope for function body.
  context.PushCleanupScope();

  // Process body statements via HandleCodeBlock which handles source ordering
  // and condition expression association for if/while/guard.
  if (body_code_block.has_value()) {
    HandleCodeBlock(context, body_code_block);
  }

  // Implicit return: if the function has a non-void return type and the
  // current block is not terminated, the last expression's value is returned.
  if (!context.IsCurrentBlockTerminated()) {
    auto& fn = context.functions().Get(function_id);
    if (fn.return_type_inst_id.has_value()) {
      auto last_inst = context.GetLastInstInCurrentBlock();
      if (last_inst.has_value()) {
        auto last = context.insts().Get(last_inst);
        if (last.type_id().has_value() &&
            last.type_id() != SemIR::ErrorInst::TypeId) {
          context.AddInst(SemIR::LocIdAndInst(
              SemIR::LocId(body_code_block.has_value() ? body_code_block
                                                       : node_id),
              SemIR::ReturnExpr{
                  .expr_id = last_inst,
                  .dest_id =
                      SemIR::DestInstId(SemIR::InstId::None)}));
        }
      }
    }
  }

  // M78: Pop ARC cleanup scope (emits releases for any remaining class locals).
  if (body_code_block.has_value()) {
    context.PopCleanupScope(body_code_block);
  } else {
    context.PopCleanupScope(node_id);
  }

  context.PopInstBlock();

  // M51: Clear this function's deferred blocks (they have been emitted at each
  // return site already).
  context.ClearDeferredBlocks();

  // M51/M52: Restore outer function's deferred blocks.
  for (auto block : outer_deferred_blocks) {
    context.PushDeferredBlock(block);
  }

  // M52: Restore outer function instead of clearing (supports nested functions).
  context.SetCurrentFunction(outer_function_id);

  context.PopScope();
}

auto HandleFunctionDecl(Context& context, Parse::NodeId node_id) -> void {
  // Forward declaration - extract signature and register name.
  auto sig = ExtractFunctionSignature(context, node_id);

  // M74: Consume pending access level.
  auto access_level = context.TakePendingAccessLevel();

  // M75/M76: Consume pending attribute.
  auto attr_node = context.TakePendingAttribute();
  auto attr_info = ExtractAttributeInfo(context, attr_node);

  // M66-M68: Detect generic parameters for forward declarations too.
  auto generic_params = ExtractGenericParams(context, node_id);

  SemIR::Function fn;
  fn.name_id = sig.name_id;
  fn.parent_scope_id = context.CurrentScopeId();
  fn.param_default_nodes = sig.param_defaults;
  fn.access_level = access_level;
  fn.is_extern_c = attr_info.is_extern_c;
  fn.is_cdecl = attr_info.is_cdecl;
  fn.cdecl_name = attr_info.cdecl_name;
  if (attr_info.is_extern_c && fn.name_id.has_value()) {
    // Use the function's own name as the extern symbol name.
    auto ident_id = fn.name_id.AsIdentifierId();
    if (ident_id.has_value()) {
      fn.extern_name = context.identifiers().Get(ident_id).str();
    }
  }

  if (!generic_params.empty()) {
    fn.is_generic_template = true;
    fn.generic_params = generic_params;
    fn.template_sig_node_id = node_id;
    fn.source_check_ir_id = context.current_file_id();
  }

  if (sig.return_type_id.has_value()) {
    fn.return_type_inst_id = context.types().GetTypeInstId(sig.return_type_id);
  }

  // M75: For @extern("C") (bodyless) functions, create parameter instructions
  // so callers can resolve parameter counts and types correctly.
  if (!sig.params.empty()) {
    llvm::SmallVector<SemIR::InstId> param_pattern_ids;
    llvm::SmallVector<SemIR::InstId> param_ids;

    for (size_t i = 0; i < sig.params.size(); ++i) {
      auto [param_name_id, param_type_id] = sig.params[i];
      auto param_index = SemIR::CallParamIndex(static_cast<int32_t>(i));

      auto entity_name_id = context.entity_names().Add(
          {.name_id = param_name_id,
           .parent_scope_id = context.CurrentScopeId()});

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

      bool is_inout = (i < sig.param_is_inout.size()) && sig.param_is_inout[i];
      SemIR::InstId param_id = SemIR::InstId::None;
      if (is_inout) {
        param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                        : node_id),
            SemIR::InoutParam{.type_id = param_type_id,
                              .index = param_index,
                              .name_id = param_name_id}));
      } else {
        param_id = context.AddInstInNoBlock(SemIR::LocIdAndInst(
            SemIR::LocId(sig.name_node_id.has_value() ? sig.name_node_id
                                                        : node_id),
            SemIR::ValueParam{.type_id = param_type_id,
                              .index = param_index,
                              .pretty_name_id = param_name_id}));
      }
      param_ids.push_back(param_id);
    }

    if (!param_ids.empty()) {
      auto params_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          params_block_id, llvm::ArrayRef<SemIR::InstId>(param_ids));
      fn.call_params_id = params_block_id;
    }
    if (!param_pattern_ids.empty()) {
      auto patterns_block_id = context.inst_blocks().AddPlaceholder();
      context.inst_blocks().ReplacePlaceholder(
          patterns_block_id,
          llvm::ArrayRef<SemIR::InstId>(param_pattern_ids));
      fn.call_param_patterns_id = patterns_block_id;
    }
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

  // M74: Record access level for the declaration.
  context.SetAccessLevel(fn_decl_id, access_level, context.CurrentScopeId());
}

}  // namespace TinySwift::Check
