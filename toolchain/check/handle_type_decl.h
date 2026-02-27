// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_TYPE_DECL_H_
#define TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_TYPE_DECL_H_

#include "toolchain/check/context.h"
#include "toolchain/parse/tree_and_subtrees.h"

namespace TinySwift::Check {

// Field information collected from struct/class var declarations.
struct FieldInfo {
  SemIR::NameId name_id;
  SemIR::TypeId type_id;
  Parse::NodeId node_id;
};

auto HandleStructDefinition(Context& context, Parse::NodeId node_id) -> void;
auto HandleClassDefinition(Context& context, Parse::NodeId node_id) -> void;
auto HandleEnumDefinition(Context& context, Parse::NodeId node_id) -> void;
auto HandleProtocolDefinition(Context& context, Parse::NodeId node_id) -> void;
auto HandleExtensionDefinition(Context& context, Parse::NodeId node_id) -> void;
auto HandleTypealiasDecl(Context& context, Parse::NodeId node_id) -> void;

}  // namespace TinySwift::Check

#endif  // TINYSWIFT_TOOLCHAIN_CHECK_HANDLE_TYPE_DECL_H_
