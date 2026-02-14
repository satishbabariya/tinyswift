// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

auto DriverSubcommand::TestAndDiagnoseIfFuzzingExternalLibraries(
    DriverEnv& driver_env, llvm::StringRef name) -> bool {
  // Only need to do anything when fuzzing.
  if (!driver_env.fuzzing) {
    return false;
  }

  TINYSWIFT_DIAGNOSTIC(
      ToolFuzzingDisallowed, Error,
      "preventing fuzzing of `{0}` subcommand due to external library",
      std::string);
  driver_env.emitter.Emit(ToolFuzzingDisallowed, name.str());
  return true;
}

}  // namespace TinySwift
