// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/language_server_subcommand.h"

#include "toolchain/language_server/language_server.h"

namespace TinySwift {

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "language-server",
    .help = R"""(
Run the TinySwift Language Server Protocol (LSP) server.

This subcommand starts an LSP server that communicates over stdin/stdout
using the JSON-RPC protocol. It provides real-time diagnostics (errors and
warnings) as files are edited.

Intended for use by editors (VS Code, Neovim, Zed, etc.) rather than
direct invocation.
)""",
};

LanguageServerSubcommand::LanguageServerSubcommand()
    : DriverSubcommand(SubcommandInfo) {}

auto LanguageServerSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  if (TestAndDiagnoseIfFuzzingExternalLibraries(driver_env,
                                                "language-server")) {
    return {.success = false};
  }

  if (!driver_env.input_stream) {
    TINYSWIFT_DIAGNOSTIC(LanguageServerMissingInputStream, Error,
                         "language server requires stdin");
    driver_env.emitter.Emit(LanguageServerMissingInputStream);
    return {.success = false};
  }

  bool success = LanguageServer::Run(
      driver_env.input_stream, *driver_env.output_stream,
      *driver_env.error_stream, driver_env.vlog_stream, driver_env.consumer);
  return {.success = success};
}

}  // namespace TinySwift
