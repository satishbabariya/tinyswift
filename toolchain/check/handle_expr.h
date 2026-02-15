// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_EXPR_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_EXPR_H_

#include "toolchain/check/context.h"
#include "toolchain/parse/tree_and_subtrees.h"

namespace TinySwift::Check {

// Evaluates an expression parse node and returns its SemIR instruction ID.
auto HandleExpr(Context& context, Parse::NodeId node_id) -> SemIR::InstId;

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_EXPR_H_
