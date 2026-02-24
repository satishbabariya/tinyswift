// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_FUNCTION_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_FUNCTION_H_

#include "toolchain/check/context.h"
#include "toolchain/parse/tree_and_subtrees.h"

namespace TinySwift::Check {

auto HandleFunctionDefinition(Context& context, Parse::NodeId node_id,
                              bool is_static_hint = false,
                              bool is_mutating_hint = false) -> void;
auto HandleFunctionDecl(Context& context, Parse::NodeId node_id) -> void;

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_FUNCTION_H_
