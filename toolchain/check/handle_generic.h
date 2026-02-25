// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_GENERIC_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_GENERIC_H_

#include "toolchain/check/context.h"
#include "toolchain/sem_ir/function.h"
#include "toolchain/sem_ir/ids.h"

namespace TinySwift::Check {

// Extracts generic type arguments from a CallExprStart node's children.
// Looks for GenericArgumentClause containing concrete type nodes.
auto ExtractGenericArgs(Context& context, Parse::NodeId call_start_node)
    -> llvm::SmallVector<SemIR::TypeId>;

// Builds a cache key string for a function specialization.
// E.g., "identity_Int" for identity<Int>.
auto BuildSpecializationCacheKey(Context& context,
                                 const SemIR::Function& fn,
                                 llvm::ArrayRef<SemIR::TypeId> type_args)
    -> std::string;

// Specializes a generic function template with concrete type arguments.
// Re-runs HandleFunctionDefinition on the template's parse tree with
// type parameter substitutions. Returns the new concrete FunctionId.
auto SpecializeGenericFunction(Context& context,
                               SemIR::FunctionId template_fn_id,
                               llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::FunctionId;

// Handles a call to a generic function: extracts type args, specializes,
// and emits the concrete Call instruction.
auto HandleGenericCallExpr(Context& context,
                           Parse::NodeId node_id,
                           SemIR::FunctionId template_fn_id,
                           SemIR::InstId callee_decl_id,
                           llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::InstId;

// M69: Specializes a generic struct/class/enum type template.
auto SpecializeGenericType(Context& context,
                           int32_t template_name_index,
                           llvm::ArrayRef<SemIR::TypeId> type_args)
    -> SemIR::InstId;

// M70: Verifies that concrete type args satisfy generic constraints.
auto VerifyGenericConstraints(Context& context,
                              const SemIR::Function& fn,
                              llvm::ArrayRef<SemIR::TypeId> type_args,
                              Parse::NodeId call_node_id) -> bool;

// M72: Infers generic type arguments from concrete call arguments.
auto InferTypeArgs(Context& context,
                   const SemIR::Function& fn,
                   llvm::ArrayRef<std::pair<llvm::StringRef, SemIR::InstId>> labeled_args)
    -> llvm::SmallVector<SemIR::TypeId>;

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_GENERIC_H_
