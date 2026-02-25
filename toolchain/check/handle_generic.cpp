// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/check/handle_generic.h"

#include "toolchain/check/handle_expr.h"
#include "toolchain/check/handle_function.h"
#include "toolchain/check/handle_type.h"
#include "toolchain/check/handle_type_decl.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"
#include "toolchain/sem_ir/typed_insts.h"

namespace TinySwift::Check {

namespace {
TINYSWIFT_DIAGNOSTIC(GenericArgCountMismatch, Error,
                     "generic type expects {0} type argument(s), got {1}",
                     int, int);
TINYSWIFT_DIAGNOSTIC(TypeDoesNotConformToProtocol, Error,
                     "type '{0}' does not conform to protocol '{1}'",
                     std::string, std::string);
}  // namespace

auto ExtractGenericArgs(Context& context, Parse::NodeId call_start_node)
    -> llvm::SmallVector<SemIR::TypeId> {
  llvm::SmallVector<SemIR::TypeId> result;
  auto children = context.children_source_order(call_start_node);

  for (auto child : children) {
    auto child_kind = context.node_kind(child);
    if (child_kind == Parse::NodeKind::GenericArgumentClause) {
      auto clause_children = context.children_source_order(child);
      for (auto cc : clause_children) {
        auto cc_kind = context.node_kind(cc);
        if (cc_kind == Parse::NodeKind::GenericArgumentClauseStart ||
            cc_kind == Parse::NodeKind::PatternListComma) {
          continue;
        }
        // Type nodes (IdentifierType, OptionalType, ArrayType, etc.)
        if (cc_kind.category().HasAnyOf(Parse::NodeCategory::Type)) {
          auto type_id = HandleTypeExpr(context, cc);
          result.push_back(type_id);
        }
      }
      break;
    }
  }
  return result;
}

auto BuildSpecializationCacheKey(Context& context,
                                 const SemIR::Function& fn,
                                 llvm::ArrayRef<SemIR::TypeId> type_args)
    -> std::string {
  std::string key;
  if (fn.name_id.has_value()) {
    auto ident_opt = fn.name_id.AsIdentifierId();
    if (ident_opt.has_value()) {
      key = context.identifiers().Get(ident_opt).str();
    }
  }
  for (auto type_arg : type_args) {
    key += "_";
    key += context.GetTypeName(type_arg);
  }
  return key;
}

auto SpecializeGenericFunction(Context& context,
                               SemIR::FunctionId template_fn_id,
                               llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::FunctionId {
  auto& template_fn = context.functions().Get(template_fn_id);

  // Build cache key and check for existing specialization.
  auto cache_key = BuildSpecializationCacheKey(context, template_fn, type_args);
  auto cache_it = context.specialization_cache().find(cache_key);
  if (cache_it != context.specialization_cache().end()) {
    return cache_it->second;
  }

  // Save context state before re-entry (M52 nested function pattern).
  auto saved_function_id = context.CurrentFunctionId();
  auto saved_deferred = context.GetDeferredBlocks();
  context.ClearDeferredBlocks();
  auto saved_type_param_bindings = context.type_param_bindings();
  auto saved_is_specializing = context.is_specializing();

  // Bind type parameters to concrete types.
  context.type_param_bindings().clear();
  context.set_is_specializing(true);

  for (size_t i = 0; i < template_fn.generic_params.size() && i < type_args.size(); ++i) {
    auto param_name_id = template_fn.generic_params[i].name_id;
    if (param_name_id.has_value()) {
      context.type_param_bindings().insert({param_name_id.index, type_args[i]});
    }
  }

  // Re-run HandleFunctionDefinition on the template's parse tree.
  // The type param bindings will cause HandleTypeExpr to resolve "T" -> "Int".
  auto pre_count = static_cast<int32_t>(context.functions().size());
  HandleFunctionDefinition(context, template_fn.template_node_id);
  auto post_count = static_cast<int32_t>(context.functions().size());

  if (post_count <= pre_count) {
    // Restore context state.
    context.type_param_bindings() = saved_type_param_bindings;
    context.set_is_specializing(saved_is_specializing);
    context.SetCurrentFunction(saved_function_id);
    context.ClearDeferredBlocks();
    for (auto block : saved_deferred) {
      context.PushDeferredBlock(block);
    }
    return template_fn_id;  // fallback
  }

  // The newly created function is the LAST function in the store.
  // Patch its name to the mangled specialization name.
  // CRITICAL: Must use GetIdTag().Apply() to create a properly tagged FunctionId.
  auto new_fn_id = context.functions().GetIdTag().Apply(pre_count);
  auto& new_fn = context.functions().Get(new_fn_id);

  // Build a persistent mangled name. Use AllocateString to ensure the
  // StringRef outlives the check Context (stored in SharedValueStores' allocator).
  auto stable_name = context.AllocateString(llvm::StringRef(cache_key));
  auto mangled_ident_id = context.identifiers().Add(stable_name);
  new_fn.name_id = SemIR::NameId::ForIdentifier(mangled_ident_id);
  new_fn.is_generic_template = false;  // Concrete specialization, not template.

  // Restore context state.
  context.type_param_bindings() = saved_type_param_bindings;
  context.set_is_specializing(saved_is_specializing);
  context.SetCurrentFunction(saved_function_id);
  context.ClearDeferredBlocks();
  for (auto block : saved_deferred) {
    context.PushDeferredBlock(block);
  }

  // Cache the specialization.
  context.specialization_cache().insert({cache_key, new_fn_id});

  return new_fn_id;
}

auto HandleGenericCallExpr(Context& context,
                           Parse::NodeId node_id,
                           SemIR::FunctionId template_fn_id,
                           SemIR::InstId callee_decl_id,
                           llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::InstId {
  auto& template_fn = context.functions().Get(template_fn_id);

  // Verify arg count matches param count.
  if (type_args.size() != template_fn.generic_params.size()) {
    context.EmitError(node_id, GenericArgCountMismatch,
                      static_cast<int>(template_fn.generic_params.size()),
                      static_cast<int>(type_args.size()));
    return context.AddInst(SemIR::LocIdAndInst::NoLoc(
        SemIR::ErrorInst{SemIR::ErrorInst::TypeId}));
  }

  // M70: Verify constraints.
  VerifyGenericConstraints(context, template_fn, type_args, node_id);

  // Specialize the function.
  auto specialized_fn_id = SpecializeGenericFunction(context, template_fn_id, type_args);

  // Get the FunctionDecl InstId for the specialized function.
  // Find it by looking up the specialized function's decl_block_id.
  auto& specialized_fn = context.functions().Get(specialized_fn_id);

  // Return the specialized FunctionId so the caller can emit the Call.
  // We use an approach that returns a sentinel: the caller (HandleCallExpr)
  // will detect `is_generic_template` on the original and switch to the
  // specialized function for the actual Call emission.
  // For now, just return the specialized_fn_id via context.
  // Actually, we return the specialized FunctionDecl instruction id.
  // We need to find it. The function decl for the specialized function was
  // emitted by HandleFunctionDefinition, and it's in scope. We can look it up.

  // Actually, the simplest approach: emit the Call instruction here directly.
  // We need the specialized function's return type.
  auto type_id = SemIR::ErrorInst::TypeId;
  if (specialized_fn.return_type_inst_id.has_value()) {
    type_id = context.types().GetTypeIdForTypeInstId(
        specialized_fn.return_type_inst_id);
  }

  return context.AddInst(SemIR::LocIdAndInst(
      SemIR::LocId(node_id),
      SemIR::Call{.type_id = type_id,
                  .callee_id = callee_decl_id,
                  .args_id = SemIR::InstBlockId::Empty}));
}

// M69: Specialize a generic type template.
auto SpecializeGenericType(Context& context,
                           int32_t template_name_index,
                           llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::InstId {
  auto tmpl_it = context.generic_type_templates().find(template_name_index);
  if (tmpl_it == context.generic_type_templates().end()) {
    return SemIR::InstId::None;
  }

  auto& tmpl = tmpl_it->second;

  // Build cache key.
  std::string cache_key;
  auto name_id = SemIR::NameId(template_name_index);
  auto ident_opt = name_id.AsIdentifierId();
  if (ident_opt.has_value()) {
    cache_key = context.identifiers().Get(ident_opt).str();
  }
  for (auto ta : type_args) {
    cache_key += "_";
    cache_key += context.GetTypeName(ta);
  }

  auto cache_it = context.type_specialization_cache().find(cache_key);
  if (cache_it != context.type_specialization_cache().end()) {
    return cache_it->second;
  }

  // Save context state.
  auto saved_bindings = context.type_param_bindings();
  auto saved_specializing = context.is_specializing();
  auto saved_type_id = context.CurrentTypeInstId();

  // Bind type parameters.
  context.type_param_bindings().clear();
  context.set_is_specializing(true);

  for (size_t i = 0; i < tmpl.generic_param_names.size() && i < type_args.size(); ++i) {
    auto pname_id = tmpl.generic_param_names[i].name_id;
    if (pname_id.has_value()) {
      context.type_param_bindings().insert({pname_id.index, type_args[i]});
    }
  }

  // Determine what kind of type definition this is and re-run the handler.
  auto def_kind = context.node_kind(tmpl.definition_node_id);
  if (def_kind == Parse::NodeKind::StructDefinition) {
    HandleStructDefinition(context, tmpl.definition_node_id);
  } else if (def_kind == Parse::NodeKind::ClassDefinition) {
    HandleClassDefinition(context, tmpl.definition_node_id);
  } else if (def_kind == Parse::NodeKind::EnumDefinition) {
    HandleEnumDefinition(context, tmpl.definition_node_id);
  }

  // The newly created type is the last instruction (the struct/class/enum inst).
  // Find it by scanning backwards from the end of the inst store.
  // CRITICAL: Must use GetIdTag().Apply() to create properly tagged InstIds.
  SemIR::InstId new_type_inst_id = SemIR::InstId::None;
  auto inst_tag = context.insts().GetIdTag();
  int num_insts = context.insts().size();
  for (int idx = num_insts - 1; idx >= 0; --idx) {
    auto tagged_id = inst_tag.Apply(idx);
    auto inst = context.insts().Get(tagged_id);
    if (inst.Is<SemIR::StructType>() || inst.Is<SemIR::ClassType>() ||
        inst.Is<SemIR::EnumDecl>()) {
      new_type_inst_id = tagged_id;
      break;
    }
  }

  // Rename the specialized type with a mangled name scope.
  if (new_type_inst_id.has_value()) {
    auto inst = context.insts().Get(new_type_inst_id);
    SemIR::NameScopeId scope_id = SemIR::NameScopeId::None;
    if (auto st = inst.TryAs<SemIR::StructType>()) {
      scope_id = st->name_scope_id;
    } else if (auto ct = inst.TryAs<SemIR::ClassType>()) {
      scope_id = ct->name_scope_id;
    } else if (auto ed = inst.TryAs<SemIR::EnumDecl>()) {
      scope_id = ed->name_scope_id;
    }

    if (scope_id.has_value()) {
      auto stable_name = context.AllocateString(llvm::StringRef(cache_key));
      auto mangled_ident_id = context.identifiers().Add(stable_name);
      context.name_scopes().Get(scope_id).name_id =
          SemIR::NameId::ForIdentifier(mangled_ident_id);
    }
  }

  // Restore context state.
  context.type_param_bindings() = saved_bindings;
  context.set_is_specializing(saved_specializing);
  context.SetCurrentType(saved_type_id);

  // Cache and return.
  if (new_type_inst_id.has_value()) {
    context.type_specialization_cache().insert({cache_key, new_type_inst_id});
  }

  return new_type_inst_id;
}

// M70: Verify generic constraints.
auto VerifyGenericConstraints(Context& context,
                              const SemIR::Function& fn,
                              llvm::ArrayRef<SemIR::TypeId> type_args,
                              Parse::NodeId call_node_id) -> bool {
  bool all_satisfied = true;

  for (size_t i = 0; i < fn.generic_params.size() && i < type_args.size(); ++i) {
    auto constraint_name_id = fn.generic_params[i].constraint_name_id;
    if (!constraint_name_id.has_value()) {
      continue;  // No constraint on this type parameter.
    }

    // Look up the protocol by name.
    auto protocol_inst_id = context.LookupName(constraint_name_id);
    if (!protocol_inst_id.has_value()) {
      continue;  // Protocol not found — skip (warning would be nice but not blocking).
    }

    // Get the protocol's scope.
    auto protocol_inst = context.insts().Get(protocol_inst_id);
    SemIR::NameScopeId protocol_scope_id = SemIR::NameScopeId::None;
    // Protocols use StructType-like storage (from M59).
    if (auto st = protocol_inst.TryAs<SemIR::StructType>()) {
      protocol_scope_id = st->name_scope_id;
    }
    if (!protocol_scope_id.has_value()) {
      continue;
    }

    // Get the concrete type's scope.
    auto type_inst_id = context.types().GetTypeInstId(type_args[i]);
    if (!type_inst_id.has_value()) {
      continue;
    }
    auto type_inst = context.insts().Get(type_inst_id);
    SemIR::NameScopeId type_scope_id = SemIR::NameScopeId::None;
    if (auto st = type_inst.TryAs<SemIR::StructType>()) {
      type_scope_id = st->name_scope_id;
    } else if (auto ct = type_inst.TryAs<SemIR::ClassType>()) {
      type_scope_id = ct->name_scope_id;
    }
    if (!type_scope_id.has_value()) {
      continue;  // Builtin types don't have scopes — skip for now.
    }

    // Check that all required methods in the protocol scope exist in the type scope.
    auto& protocol_scope = context.name_scopes().Get(protocol_scope_id);
    auto& type_scope = context.name_scopes().Get(type_scope_id);
    for (auto& [proto_name_idx, proto_inst_id] : protocol_scope.names) {
      auto proto_inst = context.insts().Get(proto_inst_id);
      if (proto_inst.Is<SemIR::FunctionDecl>()) {
        // Check if the type has a matching method name.
        if (type_scope.names.find(proto_name_idx) == type_scope.names.end()) {
          auto constraint_ident = constraint_name_id.AsIdentifierId();
          std::string protocol_name = constraint_ident.has_value()
              ? context.identifiers().Get(constraint_ident).str()
              : "<unknown>";
          context.EmitError(call_node_id, TypeDoesNotConformToProtocol,
                            context.GetTypeName(type_args[i]), protocol_name);
          all_satisfied = false;
        }
      }
    }
  }

  return all_satisfied;
}

// M72: Infer generic type arguments from call arguments.
auto InferTypeArgs(Context& context,
                   const SemIR::Function& fn,
                   llvm::ArrayRef<std::pair<llvm::StringRef, SemIR::InstId>> labeled_args)
    -> llvm::SmallVector<SemIR::TypeId> {
  llvm::SmallVector<SemIR::TypeId> inferred(fn.generic_params.size(),
                                             SemIR::TypeId::None);

  if (!fn.template_sig_node_id.has_value()) {
    return {};
  }

  // Strategy: match each function parameter's type annotation text against
  // generic parameter names. If they match, infer from the corresponding
  // argument's type.
  auto sig_children = context.children_source_order(fn.template_sig_node_id);
  int param_idx = 0;

  for (auto child : sig_children) {
    auto child_kind = context.node_kind(child);
    if (child_kind != Parse::NodeKind::ExplicitParamList) continue;

    auto param_children = context.children_source_order(child);
    for (auto pc : param_children) {
      if (context.node_kind(pc) != Parse::NodeKind::FunctionParam) continue;

      // Look for TypeAnnotation in FunctionParam children.
      auto fp_children = context.children_source_order(pc);
      for (auto fpc : fp_children) {
        auto fpc_kind = context.node_kind(fpc);
        if (fpc_kind == Parse::NodeKind::TypeAnnotation) {
          // Get the type name text from TypeAnnotation's child.
          auto ta_children = context.children_source_order(fpc);
          for (auto tac : ta_children) {
            if (context.node_kind(tac) == Parse::NodeKind::IdentifierType) {
              auto type_text = context.token_text(context.node_token(tac));
              // Check if this type name matches any generic parameter.
              for (size_t gi = 0; gi < fn.generic_params.size(); ++gi) {
                auto gp_name_id = fn.generic_params[gi].name_id;
                if (!gp_name_id.has_value()) continue;
                auto gp_ident = gp_name_id.AsIdentifierId();
                if (!gp_ident.has_value()) continue;
                auto gp_text = context.identifiers().Get(gp_ident);
                if (type_text == gp_text) {
                  // Infer from the corresponding argument.
                  if (param_idx < static_cast<int>(labeled_args.size())) {
                    auto arg_id = labeled_args[param_idx].second;
                    if (arg_id.has_value()) {
                      auto arg_type = context.insts().Get(arg_id).type_id();
                      if (arg_type.has_value()) {
                        if (!inferred[gi].has_value()) {
                          inferred[gi] = arg_type;
                        }
                        // If already inferred, verify consistency.
                        // (skip for now — first inference wins)
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      ++param_idx;
    }
  }

  // Check all params are inferred.
  for (size_t i = 0; i < inferred.size(); ++i) {
    if (!inferred[i].has_value()) {
      return {};  // Could not fully infer — return empty.
    }
  }

  return inferred;
}

}  // namespace TinySwift::Check
