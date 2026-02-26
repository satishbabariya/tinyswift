// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/format_subcommand.h"

#include <string>
#include <system_error>

#include "llvm/Support/raw_ostream.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/format/format.h"
#include "toolchain/lex/lex.h"
#include "toolchain/source/source_buffer.h"

namespace TinySwift {

auto FormatOptions::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddStringPositionalArg(
      {
          .name = "FILE",
          .help = R"""(
The input source file(s) to format.
)""",
      },
      [&](auto& arg_b) {
        arg_b.Required(true);
        arg_b.Append(&input_filenames);
      });

  b.AddFlag(
      {
          .name = "in-place",
          .short_name = "i",
          .help = "Format file(s) in place, overwriting the original.",
      },
      [&](auto& arg_b) { arg_b.Set(&in_place); });
}

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "format",
    .help = R"""(
Format Swift source code.

This subcommand reformats Swift source files using consistent style rules
including indentation, spacing, and line breaks. The formatted output is
written to standard output by default, or back to the original file with
the --in-place flag.
)""",
};

FormatSubcommand::FormatSubcommand() : DriverSubcommand(SubcommandInfo) {}

auto FormatSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  bool success = true;

  for (const auto& filename : options_.input_filenames) {
    // Load the source file.
    auto source = SourceBuffer::MakeFromFileOrStdin(
        *driver_env.fs, filename, driver_env.consumer);
    if (!source) {
      success = false;
      continue;
    }

    // Tokenize.
    SharedValueStores value_stores;
    Lex::LexOptions lex_options;
    lex_options.consumer = &driver_env.consumer;
    auto tokens = Lex::Lex(value_stores, *source, lex_options);

    if (options_.in_place && source->is_regular_file()) {
      // Format to a string buffer first, then write back to file.
      std::string formatted;
      llvm::raw_string_ostream string_out(formatted);
      if (!Format::Format(tokens, string_out)) {
        success = false;
        continue;
      }

      std::error_code ec;
      llvm::raw_fd_ostream file_out(filename, ec, llvm::sys::fs::OF_None);
      if (ec) {
        TINYSWIFT_DIAGNOSTIC(FormatOutputFileError, Error,
                             "could not open file for writing `{0}`: {1}",
                             std::string, std::string);
        driver_env.emitter.Emit(FormatOutputFileError, filename.str(),
                                ec.message());
        success = false;
        continue;
      }
      file_out << formatted;
    } else {
      // Format directly to stdout.
      if (!Format::Format(tokens, *driver_env.output_stream)) {
        success = false;
      }
    }
  }

  return {.success = success};
}

}  // namespace TinySwift
