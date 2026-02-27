// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_BUILD_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_BUILD_SUBCOMMAND_H_

#include <string>

#include "common/command_line.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"
#include "toolchain/driver/manifest.h"

namespace TinySwift {

struct BuildCmdOptions {
  auto Build(CommandLine::CommandBuilder& b) -> void;

  bool release = false;
  bool verbose = false;
  llvm::StringRef target;
};

class BuildSubcommand : public DriverSubcommand {
 public:
  explicit BuildSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& b) -> void override {
    options_.Build(b);
  }

  auto Run(DriverEnv& driver_env) -> DriverResult override;

  // Exposed for RunSubcommand and TestSubcommand to reuse.
  auto RunBuild(DriverEnv& driver_env, const Manifest& manifest,
                llvm::StringRef build_dir) -> DriverResult;

  // Returns the path to the built executable for the given manifest.
  auto GetExecutablePath(const Manifest& manifest,
                         llvm::StringRef build_dir) -> std::string;

 private:
  auto FindAndParseManifest() -> std::optional<Manifest>;
  auto BuildTarget(DriverEnv& driver_env, const Manifest& manifest,
                   const ManifestTarget& target,
                   llvm::StringRef build_dir,
                   const llvm::SmallVector<std::string>& dep_archives)
      -> bool;

  BuildCmdOptions options_;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_BUILD_SUBCOMMAND_H_
