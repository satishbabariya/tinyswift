// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_CLANG_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_CLANG_SUBCOMMAND_H_

#include "common/command_line.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

// Execute a cc1 command in-process. This is called when the busybox is
// re-invoked for a cc1 job by the Clang driver's out-of-process execution.
// Returns the exit code.
auto ExecuteCC1InProcess(llvm::ArrayRef<const char*> argv) -> int;

class ClangSubcommand : public DriverSubcommand {
 public:
  explicit ClangSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& b) -> void override;

  auto Run(DriverEnv& driver_env) -> DriverResult override;

 private:
  llvm::SmallVector<llvm::StringRef> args_;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_CLANG_SUBCOMMAND_H_
