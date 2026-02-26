// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_FORMAT_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_FORMAT_SUBCOMMAND_H_

#include "common/command_line.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

// Options for the format subcommand.
struct FormatOptions {
  auto Build(CommandLine::CommandBuilder& b) -> void;

  llvm::SmallVector<llvm::StringRef> input_filenames;
  bool in_place = false;
};

// Implements the format subcommand of the driver.
class FormatSubcommand : public DriverSubcommand {
 public:
  explicit FormatSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& b) -> void override {
    options_.Build(b);
  }

  auto Run(DriverEnv& driver_env) -> DriverResult override;

 private:
  FormatOptions options_;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_FORMAT_SUBCOMMAND_H_
