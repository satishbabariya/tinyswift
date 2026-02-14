// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_BASE_LLVM_TOOLS_H_
#define TINYSWIFT_TOOLCHAIN_BASE_LLVM_TOOLS_H_

#include <cstdint>

#include "common/command_line.h"
#include "common/enum_base.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/LLVMDriver.h"

namespace TinySwift {

TINYSWIFT_DEFINE_RAW_ENUM_CLASS(LLVMTool, uint8_t) {
#define TINYSWIFT_LLVM_TOOL(Identifier, Name, BinName, MainFn) \
  TINYSWIFT_RAW_ENUM_ENUMERATOR(Identifier)
#include "toolchain/base/llvm_tools.def"
};

// An enum-like class for each of the LLVM tools.
//
// This can be used like an enum to track a specific one of the LLVM tools. It
// also has a collection of methods to access various aspects of the tools
// themselves, including the symbol used to invoke the given tool.
//
// The instances of this class are generated from `llvm_tools.bzl`, see that
// file for more details.
class LLVMTool : public TINYSWIFT_ENUM_BASE(LLVMTool) {
 public:
#define TINYSWIFT_LLVM_TOOL(Identifier, Name, BinName, MainFn) \
  TINYSWIFT_ENUM_CONSTANT_DECL(Identifier)
#include "toolchain/base/llvm_tools.def"

  static const llvm::ArrayRef<LLVMTool> Tools;

  using MainFnT = auto(int argc, char** argv, const llvm::ToolContext& context)
      -> int;

  using EnumBase::EnumBase;

  auto bin_name() const -> llvm::StringLiteral { return BinNames[AsInt()]; }

  auto main_fn() const -> MainFnT* { return MainFns[AsInt()]; }

  auto subcommand_info() const -> const CommandLine::CommandInfo& {
    return SubcommandInfos[AsInt()];
  }

 private:
  static const LLVMTool ToolsStorage[];

  static const llvm::StringLiteral BinNames[];
  static MainFnT* const MainFns[];

  static const CommandLine::CommandInfo SubcommandInfos[];
};

#define TINYSWIFT_LLVM_TOOL(Identifier, Name, BinName, MainFn) \
  TINYSWIFT_ENUM_CONSTANT_DEFINITION(LLVMTool, Identifier)
#include "toolchain/base/llvm_tools.def"

inline constexpr LLVMTool LLVMTool::ToolsStorage[] = {
#define TINYSWIFT_LLVM_TOOL(Identifier, Name, BinName, MainFn) \
  LLVMTool::Identifier,
#include "toolchain/base/llvm_tools.def"
};
inline constexpr llvm::ArrayRef<LLVMTool> LLVMTool::Tools = ToolsStorage;

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_BASE_LLVM_TOOLS_H_
