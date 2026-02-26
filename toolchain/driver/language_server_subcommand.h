// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_LANGUAGE_SERVER_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_LANGUAGE_SERVER_SUBCOMMAND_H_

#include "common/command_line.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

// Implements the language-server subcommand of the driver.
class LanguageServerSubcommand : public DriverSubcommand {
 public:
  explicit LanguageServerSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& /*b*/) -> void override {}

  auto Run(DriverEnv& driver_env) -> DriverResult override;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_LANGUAGE_SERVER_SUBCOMMAND_H_
