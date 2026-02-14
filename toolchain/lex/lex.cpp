// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lex/lex.h"

#include "common/check.h"
#include "common/vlog.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/lex/character_set.h"
#include "toolchain/lex/token_index.h"
#include "toolchain/lex/token_info.h"
#include "toolchain/lex/token_kind.h"
#include "toolchain/lex/tokenized_buffer.h"

namespace TinySwift::Lex {

// Minimal Lexer class that has friend access to TokenizedBuffer and TokenInfo.
class Lexer {
 public:
  Lexer(SharedValueStores& value_stores, SourceBuffer& source,
        LexOptions options)
      : buffer_(value_stores, source), options_(options) {}

  auto Run() -> TokenizedBuffer {
    // TODO: Implement your language's lexer here.
    // This stage should tokenize source text into a TokenizedBuffer.
    //
    // Typical responsibilities:
    // - Skip whitespace and comments
    // - Recognize keywords, identifiers, literals
    // - Match symbols and operators
    // - Track line/column information
    // - Report lexical errors via diagnostics

    // Add the file boundary tokens that the parser expects.
    buffer_.AddToken(TokenInfo(TokenKind::FileStart,
                               /*has_leading_space=*/false, /*byte_offset=*/0));
    buffer_.AddToken(TokenInfo(
        TokenKind::FileEnd, /*has_leading_space=*/true,
        /*byte_offset=*/static_cast<int32_t>(buffer_.source().text().size())));

    return std::move(buffer_);
  }

 private:
  TokenizedBuffer buffer_;
  LexOptions options_;
};

auto Lex(SharedValueStores& value_stores, SourceBuffer& source,
         LexOptions options) -> TokenizedBuffer {
  auto* consumer =
      options.consumer ? options.consumer : &Diagnostics::ConsoleConsumer();

  Lexer lexer(value_stores, source, options);
  auto buffer = lexer.Run();

  if (options.vlog_stream || options.dump_stream) {
    consumer->Flush();
  }
  TINYSWIFT_VLOG_TO(options.vlog_stream, "*** Lex::TokenizedBuffer ***\n{0}",
                 buffer);
  if (options.dump_stream) {
    buffer.Print(*options.dump_stream, options.omit_file_boundary_tokens);
  }
  return buffer;
}

}  // namespace TinySwift::Lex
