// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/run_subcommand.h"

#include <string>

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "toolchain/driver/build_subcommand.h"
#include "toolchain/driver/manifest.h"

namespace TinySwift {

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "run",
    .help = R"""(
Build and run the project.

Compiles, links, and executes the project's executable target. Arguments
after `--` are forwarded to the program.
)""",
};

RunSubcommand::RunSubcommand() : DriverSubcommand(SubcommandInfo) {}

auto RunOptions::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddFlag(
      {
          .name = "release",
          .help = "Build in release mode.",
      },
      [&](auto& arg_b) { arg_b.Set(&release); });
  b.AddFlag(
      {
          .name = "verbose",
          .short_name = "v",
          .help = "Print commands as they are executed.",
      },
      [&](auto& arg_b) { arg_b.Set(&verbose); });
  b.AddStringPositionalArg(
      {
          .name = "ARGS",
          .help = "Arguments to pass to the program.",
      },
      [&](auto& arg_b) {
        arg_b.Required(false);
        arg_b.Append(&program_args);
      });
}

auto RunSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  // Build first.
  BuildSubcommand build_cmd;

  // Find and parse manifest.
  llvm::SmallString<256> cwd;
  if (llvm::sys::fs::current_path(cwd)) {
    TINYSWIFT_DIAGNOSTIC(RunCwdError, Error,
                      "could not determine current directory");
    driver_env.emitter.Emit(RunCwdError);
    return {.success = false};
  }

  auto manifest_path = FindManifest(cwd);
  std::optional<Manifest> manifest;
  if (manifest_path) {
    manifest = ParseManifest(*manifest_path);
  }
  if (!manifest) {
    manifest = BuildDefaultManifest(cwd);
  }

  llvm::SmallString<256> build_dir(manifest->directory);
  llvm::sys::path::append(build_dir, ".build");
  llvm::sys::path::append(build_dir,
                          options_.release ? "release" : "debug");

  auto build_result = build_cmd.RunBuild(driver_env, *manifest, build_dir);
  if (!build_result.success) {
    return build_result;
  }

  // Find the executable.
  auto exe_path = build_cmd.GetExecutablePath(*manifest, build_dir);
  if (!llvm::sys::fs::exists(exe_path)) {
    TINYSWIFT_DIAGNOSTIC(RunExeNotFound, Error,
                      "executable not found: `{0}`", std::string);
    driver_env.emitter.Emit(RunExeNotFound, exe_path);
    return {.success = false};
  }

  // Execute.
  llvm::SmallVector<llvm::StringRef> args;
  args.push_back(exe_path);
  for (const auto& arg : options_.program_args) {
    args.push_back(arg);
  }

  if (options_.verbose) {
    *driver_env.error_stream << "Running: " << exe_path;
    for (const auto& arg : options_.program_args) {
      *driver_env.error_stream << " " << arg;
    }
    *driver_env.error_stream << "\n";
  }

  std::string err_msg;
  int result = llvm::sys::ExecuteAndWait(exe_path, args,
                                         /*Env=*/std::nullopt,
                                         /*Redirects=*/{},
                                         /*SecondsToWait=*/0,
                                         /*MemoryLimit=*/0, &err_msg);

  return {.success = result == 0};
}

}  // namespace TinySwift
