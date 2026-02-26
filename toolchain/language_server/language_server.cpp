// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/language_server/language_server.h"

#include "toolchain/language_server/server.h"
#include "toolchain/language_server/transport.h"

namespace TinySwift::LanguageServer {

auto Run(FILE* input_stream, llvm::raw_ostream& output_stream,
         llvm::raw_ostream& error_stream,
         llvm::raw_ostream* /*vlog_stream*/,
         Diagnostics::Consumer& /*consumer*/) -> bool {
  Transport transport(input_stream, output_stream, error_stream);
  Server server(transport, error_stream);
  return server.Run();
}

}  // namespace TinySwift::LanguageServer
