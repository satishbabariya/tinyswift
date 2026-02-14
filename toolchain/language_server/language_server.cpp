// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/language_server/language_server.h"

#include "toolchain/diagnostics/emitter.h"

// TODO: Implement your language's LSP server here.
// See TinySwift compiler for reference implementation patterns.
//
// A typical language server implementation uses clangd's JSON transport:
// 1. Create a clangd::Transport for JSON-RPC communication
// 2. Set up a context to manage open files
// 3. Handle incoming LSP requests (textDocument/didOpen, etc.)
// 4. Run lex/parse/check on file changes
// 5. Publish diagnostics back to the client

namespace TinySwift::LanguageServer {

auto Run(FILE* /*input_stream*/, llvm::raw_ostream& /*output_stream*/,
         llvm::raw_ostream& /*error_stream*/,
         llvm::raw_ostream* /*vlog_stream*/,
         Diagnostics::Consumer& consumer) -> bool {
  TINYSWIFT_DIAGNOSTIC(LanguageServerNotImplemented, Error,
                    "language server not yet implemented");
  Diagnostics::NoLocEmitter emitter(&consumer);
  emitter.Emit(LanguageServerNotImplemented);
  return false;
}

}  // namespace TinySwift::LanguageServer
