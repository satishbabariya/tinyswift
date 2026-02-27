// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_RUN_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_RUN_SUBCOMMAND_H_

#include "common/command_line.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

struct RunOptions {
  auto Build(CommandLine::CommandBuilder& b) -> void;

  bool release = false;
  bool verbose = false;
  llvm::SmallVector<llvm::StringRef> program_args;
};

class RunSubcommand : public DriverSubcommand {
 public:
  explicit RunSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& b) -> void override {
    options_.Build(b);
  }

  auto Run(DriverEnv& driver_env) -> DriverResult override;

 private:
  RunOptions options_;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_RUN_SUBCOMMAND_H_
