// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_CODEGEN_CODEGEN_OPTIONS_H_
#define TINYSWIFT_TOOLCHAIN_CODEGEN_CODEGEN_OPTIONS_H_

#include "common/command_line.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/TargetParser/Host.h"

namespace TinySwift {

// Shared codegen-related options.
//
// See the implementation of `Build` for documentation on members.
struct CodegenOptions {
  auto Build(CommandLine::CommandBuilder& b) -> void;

  std::string host = llvm::sys::getDefaultTargetTriple();
  llvm::StringRef target;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_CODEGEN_CODEGEN_OPTIONS_H_
