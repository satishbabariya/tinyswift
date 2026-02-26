// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_TRANSPORT_H_
#define TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_TRANSPORT_H_

#include <cstdio>
#include <optional>

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

namespace TinySwift::LanguageServer {

// JSON-RPC transport over stdio with Content-Length framing.
//
// Reads and writes LSP messages using the base protocol:
//   Content-Length: <length>\r\n
//   \r\n
//   <JSON body>
class Transport {
 public:
  explicit Transport(FILE* input, llvm::raw_ostream& output,
                     llvm::raw_ostream& error);

  // Reads a single JSON-RPC message from input. Returns nullopt on EOF or
  // parse error.
  auto ReadMessage() -> std::optional<llvm::json::Value>;

  // Writes a JSON-RPC message to output with Content-Length framing.
  auto WriteMessage(const llvm::json::Value& message) -> void;

 private:
  // Reads headers and returns the Content-Length value. Returns -1 on error/EOF.
  auto ReadContentLength() -> int;

  FILE* input_;
  llvm::raw_ostream& output_;
  llvm::raw_ostream& error_;
};

}  // namespace TinySwift::LanguageServer

#endif  // TINYSWIFT_TOOLCHAIN_LANGUAGE_SERVER_TRANSPORT_H_
