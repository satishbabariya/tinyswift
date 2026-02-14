// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/driver.h"

#include <memory>

#include "common/command_line.h"
#include "common/pretty_stack_trace_function.h"
#include "common/version.h"
#include "toolchain/driver/compile_subcommand.h"

namespace TinySwift {

namespace {
struct Options {
  static const CommandLine::CommandInfo Info;

  auto Build(CommandLine::CommandBuilder& b) -> void;

  bool verbose = false;
  bool fuzzing = false;
  bool include_diagnostic_kind = false;

  CompileSubcommand compile;

  // TODO: Add additional subcommands as needed:
  // - format: Format source code
  // - language-server: LSP support
  // - link: Link compiled objects

  // On success, this is set to the subcommand to run.
  DriverSubcommand* selected_subcommand = nullptr;
};
}  // namespace

const CommandLine::CommandInfo Options::Info = {
    .name = "tinyswift",
    .version = Version::ToolchainInfo,
    .help = R"""(
This is the unified compiler toolchain driver. Its subcommands provide
all of the core behavior of the toolchain, including compilation, linking, and
developer tools. Each of these has its own subcommand, and you can pass a
specific subcommand to the `help` subcommand to get details about its usage.
)""",
};

auto Options::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddFlag(
      {
          .name = "verbose",
          .short_name = "v",
          .help = "Enable verbose logging to the stderr stream.",
      },
      [&](CommandLine::FlagBuilder& arg_b) { arg_b.Set(&verbose); });

  b.AddFlag(
      {
          .name = "fuzzing",
          .help = "Configure the command line for fuzzing.",
      },
      [&](CommandLine::FlagBuilder& arg_b) { arg_b.Set(&fuzzing); });

  b.AddFlag(
      {
          .name = "include-diagnostic-kind",
          .help = R"""(
When printing diagnostics, include the diagnostic kind as part of output.
)""",
      },
      [&](auto& arg_b) { arg_b.Set(&include_diagnostic_kind); });

  compile.AddTo(b, &selected_subcommand);

  b.RequiresSubcommand();
}

auto Driver::RunCommand(llvm::ArrayRef<llvm::StringRef> args) -> DriverResult {
  PrettyStackTraceFunction trace_version([&](llvm::raw_ostream& out) {
    out << "TinySwift version: " << Version::String << "\n";
  });

  if (driver_env_.installation->error()) {
    TINYSWIFT_DIAGNOSTIC(DriverInstallInvalid, Error, "{0}", std::string);
    driver_env_.emitter.Emit(DriverInstallInvalid,
                             driver_env_.installation->error()->str());
    return {.success = false};
  }

  Options options;

  ErrorOr<CommandLine::ParseResult> result = CommandLine::Parse(
      args, *driver_env_.output_stream, Options::Info,
      [&](CommandLine::CommandBuilder& b) { options.Build(b); });

  driver_env_.consumer.set_include_diagnostic_kind(
      options.include_diagnostic_kind);

  if (!result.ok()) {
    TINYSWIFT_DIAGNOSTIC(DriverCommandLineParseFailed, Error, "{0}", std::string);
    driver_env_.emitter.Emit(DriverCommandLineParseFailed,
                             PrintToString(result.error()));
    return {.success = false};
  } else if (*result == CommandLine::ParseResult::MetaSuccess) {
    return {.success = true};
  }

  if (options.verbose) {
    driver_env_.vlog_stream = driver_env_.error_stream;
  }
  if (options.fuzzing) {
    driver_env_.fuzzing = true;
  }

  TINYSWIFT_CHECK(options.selected_subcommand != nullptr);
  return options.selected_subcommand->Run(driver_env_);
}

}  // namespace TinySwift
