// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/parse/node_category.h"

#include "llvm/ADT/StringExtras.h"

namespace TinySwift::Parse {

TINYSWIFT_DEFINE_ENUM_MASK_NAMES(NodeCategory) {
  TINYSWIFT_NODE_CATEGORY(TINYSWIFT_ENUM_MASK_NAME_STRING)
};

}  // namespace TinySwift::Parse
