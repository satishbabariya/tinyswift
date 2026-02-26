// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/language_server/transport.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace TinySwift::LanguageServer {

Transport::Transport(FILE* input, llvm::raw_ostream& output,
                     llvm::raw_ostream& error)
    : input_(input), output_(output), error_(error) {}

auto Transport::ReadContentLength() -> int {
  int content_length = -1;

  // Read header lines until we hit the blank line separator.
  char line_buf[256];
  while (std::fgets(line_buf, sizeof(line_buf), input_)) {
    llvm::StringRef line(line_buf);

    // Strip trailing \r\n or \n.
    line = line.rtrim("\r\n");

    // Empty line marks end of headers.
    if (line.empty()) {
      break;
    }

    // Parse Content-Length header.
    if (line.consume_front("Content-Length: ")) {
      int val;
      if (!line.getAsInteger(10, val)) {
        content_length = val;
      }
    }
    // Ignore other headers (e.g. Content-Type).
  }

  return content_length;
}

auto Transport::ReadMessage() -> std::optional<llvm::json::Value> {
  int content_length = ReadContentLength();
  if (content_length < 0) {
    return std::nullopt;
  }

  // Read exactly content_length bytes.
  std::string body(content_length, '\0');
  size_t bytes_read = std::fread(body.data(), 1, content_length, input_);
  if (static_cast<int>(bytes_read) != content_length) {
    error_ << "LSP: expected " << content_length << " bytes, got "
           << bytes_read << "\n";
    return std::nullopt;
  }

  // Parse JSON.
  auto parsed = llvm::json::parse(body);
  if (!parsed) {
    error_ << "LSP: JSON parse error: "
           << llvm::toString(parsed.takeError()) << "\n";
    return std::nullopt;
  }

  return std::move(*parsed);
}

auto Transport::WriteMessage(const llvm::json::Value& message) -> void {
  std::string body;
  llvm::raw_string_ostream body_stream(body);
  body_stream << message;

  output_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  output_.flush();
}

}  // namespace TinySwift::LanguageServer
