// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_LANGUAGE_SERVER_H_
#define TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_LANGUAGE_SERVER_H_

#include "common/ostream.h"
#include "toolchain/diagnostics/consumer.h"

// TODO: Implement your language's LSP server here.
// See TinySwift compiler for reference implementation patterns.

namespace TinySwift::LanguageServer {

// Start the language server. input_stream and output_stream are used by LSP;
// error_stream is primarily for errors that don't fit into LSP. Returns true if
// the server cleanly exits.
auto Run(FILE* input_stream, llvm::raw_ostream& output_stream,
         llvm::raw_ostream& error_stream, llvm::raw_ostream* vlog_stream,
         Diagnostics::Consumer& consumer) -> bool;

}  // namespace TinySwift::LanguageServer

#endif  // TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_LANGUAGE_SERVER_H_
