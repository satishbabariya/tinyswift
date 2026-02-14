// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lex/lex.h"

#include "common/check.h"
#include "common/vlog.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "toolchain/base/shared_value_stores.h"
#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/diagnostics/emitter.h"
#include "toolchain/lex/character_set.h"
#include "toolchain/lex/token_index.h"
#include "toolchain/lex/token_info.h"
#include "toolchain/lex/token_kind.h"
#include "toolchain/lex/tokenized_buffer.h"

namespace TinySwift::Lex {

// Diagnostics used by the lexer.
// NOLINTBEGIN(readability-identifier-naming)
TINYSWIFT_DIAGNOSTIC(UnrecognizedCharacters, Error,
                     "encountered unrecognized characters while parsing");
TINYSWIFT_DIAGNOSTIC(UnterminatedBlockComment, Error,
                     "unterminated block comment");
TINYSWIFT_DIAGNOSTIC(UnterminatedString, Error, "unterminated string literal");
TINYSWIFT_DIAGNOSTIC(UnterminatedEscapedIdentifier, Error,
                     "unterminated escaped identifier");
TINYSWIFT_DIAGNOSTIC(InvalidNumberLiteral, Error,
                     "invalid digit in {0} literal", std::string);
TINYSWIFT_DIAGNOSTIC(ExpectedDigitAfterPrefix, Error,
                     "expected digit after '{0}' prefix", std::string);
TINYSWIFT_DIAGNOSTIC(InvalidDollarIdentifier, Error,
                     "invalid dollar identifier");
TINYSWIFT_DIAGNOSTIC(NulCharacterInSource, Warning,
                     "nul character in source file");
TINYSWIFT_DIAGNOSTIC(TooManyTokens, Error,
                     "file has too many tokens; limit is {0}", int);
TINYSWIFT_DIAGNOSTIC(UnmatchedOpening, Error, "unmatched opening '{0}'",
                     std::string);
TINYSWIFT_DIAGNOSTIC(UnmatchedClosing, Error, "unmatched closing '{0}'",
                     std::string);
// NOLINTEND(readability-identifier-naming)

class Lexer {
 public:
  Lexer(SharedValueStores& value_stores, SourceBuffer& source,
        Diagnostics::Consumer* consumer, LexOptions options)
      : buffer_(value_stores, source),
        value_stores_(value_stores),
        source_text_(source.text()),
        cursor_(source_text_.data()),
        end_(source_text_.data() + source_text_.size()),
        emitter_(consumer, &buffer_),
        options_(options) {}

  auto Run() -> TokenizedBuffer {
    // Add initial line info.
    buffer_.line_infos_.Add(LineInfo(0));

    // Emit FileStart.
    buffer_.AddToken(TokenInfo(TokenKind::FileStart,
                               /*has_leading_space=*/false, /*byte_offset=*/0));

    // Main lexing loop.
    while (cursor_ < end_) {
      SkipWhitespaceAndComments();
      if (cursor_ >= end_) break;

      char c = *cursor_;

      // Track leading whitespace.
      bool leading_space = has_leading_space_;
      has_leading_space_ = false;

      switch (c) {
        case '\0':
          emitter_.Emit(cursor_, NulCharacterInSource);
          ++cursor_;
          continue;

        // Identifiers and keywords.
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z':
        case '_':
          LexIdentifierOrKeyword(leading_space);
          continue;

        // Number literals.
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          LexNumber(leading_space);
          continue;

        // String literals.
        case '"':
          LexStringLiteral(leading_space, 0);
          continue;

        // Grouping symbols.
        case '(':
          EmitOneCharSymbol(TokenKind::OpenParen, leading_space);
          continue;
        case ')':
          EmitOneCharSymbol(TokenKind::CloseParen, leading_space);
          continue;
        case '{':
          EmitOneCharSymbol(TokenKind::OpenCurlyBrace, leading_space);
          continue;
        case '}':
          EmitOneCharSymbol(TokenKind::CloseCurlyBrace, leading_space);
          continue;
        case '[':
          EmitOneCharSymbol(TokenKind::OpenSquareBracket, leading_space);
          continue;
        case ']':
          EmitOneCharSymbol(TokenKind::CloseSquareBracket, leading_space);
          continue;

        // Single-char punctuators that are NOT operator characters.
        case ',':
          EmitOneCharSymbol(TokenKind::Comma, leading_space);
          continue;
        case ';':
          EmitOneCharSymbol(TokenKind::Semi, leading_space);
          continue;
        case '@':
          EmitOneCharSymbol(TokenKind::At, leading_space);
          continue;
        case '\\':
          EmitOneCharSymbol(TokenKind::Backslash, leading_space);
          continue;

        // Pound (# or #keyword).
        case '#':
          LexPound(leading_space);
          continue;

        // Dollar identifiers ($0, $foo).
        case '$':
          LexDollarIdent(leading_space);
          continue;

        // Escaped identifiers (`keyword`).
        case '`':
          LexEscapedIdentifier(leading_space);
          continue;

        // Question mark: single-char punctuator (not an operator char).
        case '?':
          EmitOneCharSymbol(TokenKind::Question, leading_space);
          continue;

        // Operator characters: these go through operator/punctuator
        // disambiguation.
        case '/': case '=': case '-': case '+': case '*': case '%':
        case '<': case '>': case '!': case '&': case '|': case '^':
        case '~': case '.': case ':':
          LexOperatorOrPunctuator(leading_space);
          continue;

        default:
          // Unknown character.
          emitter_.Emit(cursor_, UnrecognizedCharacters);
          int32_t offset = static_cast<int32_t>(cursor_ - source_text_.data());
          buffer_.AddToken(
              TokenInfo(TokenKind::Error, leading_space, /*error_length=*/1,
                        offset));
          buffer_.has_errors_ = true;
          ++cursor_;
          continue;
      }
    }

    // Match open/close tokens.
    MatchOpenCloseTokens();

    // Finalize: check token count.
    if (buffer_.token_infos_.size() >= TokenIndex::Max) {
      emitter_.Emit(end_ - 1, TooManyTokens, TokenIndex::Max);
      buffer_.has_errors_ = true;
    }

    // Emit FileEnd.
    buffer_.AddToken(TokenInfo(
        TokenKind::FileEnd, /*has_leading_space=*/true,
        /*byte_offset=*/static_cast<int32_t>(source_text_.size())));

    return std::move(buffer_);
  }

 private:
  // === Whitespace & Comments ===

  void SkipWhitespaceAndComments() {
    while (cursor_ < end_) {
      char c = *cursor_;

      if (c == ' ' || c == '\t') {
        has_leading_space_ = true;
        ++cursor_;
        continue;
      }

      if (c == '\n') {
        has_leading_space_ = true;
        at_start_of_line_ = true;
        ++cursor_;
        int32_t offset = static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.line_infos_.Add(LineInfo(offset));
        // Compute indent for the new line.
        const char* indent_start = cursor_;
        while (cursor_ < end_ && IsHorizontalWhitespace(*cursor_)) {
          ++cursor_;
        }
        auto& line = buffer_.line_infos_.Get(
            LineIndex(buffer_.line_infos_.size() - 1));
        line.indent =
            static_cast<int32_t>(cursor_ - indent_start);
        // Don't set cursor back - we consumed the indent.
        continue;
      }

      if (c == '\r') {
        has_leading_space_ = true;
        at_start_of_line_ = true;
        ++cursor_;
        // Handle \r\n as a single newline.
        if (cursor_ < end_ && *cursor_ == '\n') {
          ++cursor_;
        }
        int32_t offset = static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.line_infos_.Add(LineInfo(offset));
        // Compute indent.
        const char* indent_start = cursor_;
        while (cursor_ < end_ && IsHorizontalWhitespace(*cursor_)) {
          ++cursor_;
        }
        auto& line = buffer_.line_infos_.Get(
            LineIndex(buffer_.line_infos_.size() - 1));
        line.indent = static_cast<int32_t>(cursor_ - indent_start);
        continue;
      }

      // Line comments: //
      if (c == '/' && cursor_ + 1 < end_ && *(cursor_ + 1) == '/') {
        has_leading_space_ = true;
        int32_t comment_start =
            static_cast<int32_t>(cursor_ - source_text_.data());
        // Compute indent: distance from current line start.
        auto& cur_line = buffer_.line_infos_.Get(
            LineIndex(buffer_.line_infos_.size() - 1));
        int32_t indent = comment_start - cur_line.start;
        while (cursor_ < end_ && *cursor_ != '\n' && *cursor_ != '\r') {
          ++cursor_;
        }
        int32_t comment_end =
            static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.AddComment(indent, comment_start, comment_end);
        continue;
      }

      // Block comments: /* ... */ with nesting.
      if (c == '/' && cursor_ + 1 < end_ && *(cursor_ + 1) == '*') {
        has_leading_space_ = true;
        int32_t comment_start =
            static_cast<int32_t>(cursor_ - source_text_.data());
        auto& cur_line = buffer_.line_infos_.Get(
            LineIndex(buffer_.line_infos_.size() - 1));
        int32_t indent = comment_start - cur_line.start;
        cursor_ += 2;
        int depth = 1;
        while (cursor_ < end_ && depth > 0) {
          if (*cursor_ == '/' && cursor_ + 1 < end_ &&
              *(cursor_ + 1) == '*') {
            ++depth;
            cursor_ += 2;
          } else if (*cursor_ == '*' && cursor_ + 1 < end_ &&
                     *(cursor_ + 1) == '/') {
            --depth;
            cursor_ += 2;
          } else {
            if (*cursor_ == '\n') {
              ++cursor_;
              int32_t offset =
                  static_cast<int32_t>(cursor_ - source_text_.data());
              buffer_.line_infos_.Add(LineInfo(offset));
            } else if (*cursor_ == '\r') {
              ++cursor_;
              if (cursor_ < end_ && *cursor_ == '\n') {
                ++cursor_;
              }
              int32_t offset =
                  static_cast<int32_t>(cursor_ - source_text_.data());
              buffer_.line_infos_.Add(LineInfo(offset));
            } else {
              ++cursor_;
            }
          }
        }
        if (depth > 0) {
          emitter_.Emit(source_text_.data() + comment_start,
                        UnterminatedBlockComment);
          buffer_.has_errors_ = true;
        }
        int32_t comment_end =
            static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.AddComment(indent, comment_start, comment_end);
        continue;
      }

      // Not whitespace or comment.
      break;
    }
  }

  // === Identifier & Keyword Lexing ===

  void LexIdentifierOrKeyword(bool leading_space) {
    const char* start = cursor_;
    ++cursor_;
    while (cursor_ < end_ && IsIdentifierContinuation(*cursor_)) {
      ++cursor_;
    }
    llvm::StringRef text(start, cursor_ - start);
    int32_t offset = static_cast<int32_t>(start - source_text_.data());

    // Check if this is a keyword.
    TokenKind kind = LookupKeyword(text);
    if (kind != TokenKind::Error) {
      buffer_.AddToken(TokenInfo(kind, leading_space, offset));
      return;
    }

    // It's an identifier.
    auto id = value_stores_.identifiers().Add(text);
    buffer_.AddToken(
        TokenInfo(TokenKind::Identifier, leading_space, id.index, offset));
  }

  static auto LookupKeyword(llvm::StringRef text) -> TokenKind {
    // Use StringSwitch to map keyword spellings to their TokenKinds.
    return llvm::StringSwitch<TokenKind>(text)
        // Declaration keywords
        .Case("associatedtype", TokenKind::AssociatedtypeKeyword)
        .Case("class", TokenKind::ClassKeyword)
        .Case("deinit", TokenKind::DeinitKeyword)
        .Case("enum", TokenKind::EnumKeyword)
        .Case("extension", TokenKind::ExtensionKeyword)
        .Case("func", TokenKind::FuncKeyword)
        .Case("import", TokenKind::ImportKeyword)
        .Case("init", TokenKind::InitKeyword)
        .Case("inout", TokenKind::InoutKeyword)
        .Case("let", TokenKind::LetKeyword)
        .Case("operator", TokenKind::OperatorKeyword)
        .Case("precedencegroup", TokenKind::PrecedencegroupKeyword)
        .Case("protocol", TokenKind::ProtocolKeyword)
        .Case("struct", TokenKind::StructKeyword)
        .Case("subscript", TokenKind::SubscriptKeyword)
        .Case("typealias", TokenKind::TypealiasKeyword)
        .Case("var", TokenKind::VarKeyword)
        .Case("fileprivate", TokenKind::FileprivateKeyword)
        .Case("internal", TokenKind::InternalKeyword)
        .Case("private", TokenKind::PrivateKeyword)
        .Case("public", TokenKind::PublicKeyword)
        .Case("static", TokenKind::StaticKeyword)
        // Statement keywords
        .Case("defer", TokenKind::DeferKeyword)
        .Case("if", TokenKind::IfKeyword)
        .Case("guard", TokenKind::GuardKeyword)
        .Case("do", TokenKind::DoKeyword)
        .Case("repeat", TokenKind::RepeatKeyword)
        .Case("else", TokenKind::ElseKeyword)
        .Case("for", TokenKind::ForKeyword)
        .Case("in", TokenKind::InKeyword)
        .Case("while", TokenKind::WhileKeyword)
        .Case("return", TokenKind::ReturnKeyword)
        .Case("break", TokenKind::BreakKeyword)
        .Case("continue", TokenKind::ContinueKeyword)
        .Case("fallthrough", TokenKind::FallthroughKeyword)
        .Case("switch", TokenKind::SwitchKeyword)
        .Case("case", TokenKind::CaseKeyword)
        .Case("default", TokenKind::DefaultKeyword)
        .Case("where", TokenKind::WhereKeyword)
        .Case("catch", TokenKind::CatchKeyword)
        .Case("throw", TokenKind::ThrowKeyword)
        // Expression keywords
        .Case("as", TokenKind::AsKeyword)
        .Case("Any", TokenKind::AnyKeyword)
        .Case("false", TokenKind::FalseKeyword)
        .Case("is", TokenKind::IsKeyword)
        .Case("nil", TokenKind::NilKeyword)
        .Case("rethrows", TokenKind::RethrowsKeyword)
        .Case("super", TokenKind::SuperKeyword)
        .Case("self", TokenKind::SelfKeyword)
        .Case("Self", TokenKind::CapitalSelfKeyword)
        .Case("true", TokenKind::TrueKeyword)
        .Case("try", TokenKind::TryKeyword)
        .Case("throws", TokenKind::ThrowsKeyword)
        // Pattern keyword
        .Case("_", TokenKind::UnderscoreKeyword)
        // Default: not a keyword.
        .Default(TokenKind::Error);
  }

  // === Dollar Identifier Lexing ===

  void LexDollarIdent(bool leading_space) {
    const char* start = cursor_;
    int32_t offset = static_cast<int32_t>(start - source_text_.data());
    ++cursor_;  // skip $

    if (cursor_ >= end_ ||
        (!IsDecimalDigit(*cursor_) && !IsIdentifierStart(*cursor_))) {
      emitter_.Emit(start, InvalidDollarIdentifier);
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space, /*error_length=*/1,
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    // $0, $1, $123... or $identifier
    while (cursor_ < end_ && IsIdentifierContinuation(*cursor_)) {
      ++cursor_;
    }

    llvm::StringRef text(start, cursor_ - start);
    auto id = value_stores_.identifiers().Add(text);
    buffer_.AddToken(
        TokenInfo(TokenKind::DollarIdent, leading_space, id.index, offset));
  }

  // === Escaped Identifier Lexing ===

  void LexEscapedIdentifier(bool leading_space) {
    const char* start = cursor_;
    int32_t offset = static_cast<int32_t>(start - source_text_.data());
    ++cursor_;  // skip opening `

    const char* ident_start = cursor_;
    while (cursor_ < end_ && *cursor_ != '`' && *cursor_ != '\n' &&
           *cursor_ != '\r') {
      ++cursor_;
    }

    if (cursor_ >= end_ || *cursor_ != '`') {
      emitter_.Emit(start, UnterminatedEscapedIdentifier);
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    llvm::StringRef inner(ident_start, cursor_ - ident_start);
    ++cursor_;  // skip closing `

    auto id = value_stores_.identifiers().Add(inner);
    buffer_.AddToken(
        TokenInfo(TokenKind::EscapedIdentifier, leading_space, id.index,
                  offset));
  }

  // === Number Lexing ===

  void LexNumber(bool leading_space) {
    const char* start = cursor_;
    int32_t offset = static_cast<int32_t>(start - source_text_.data());

    if (*cursor_ == '0' && cursor_ + 1 < end_) {
      char next = *(cursor_ + 1);
      if (next == 'x' || next == 'X') {
        cursor_ += 2;
        LexHexNumber(start, leading_space, offset);
        return;
      }
      if (next == 'o' || next == 'O') {
        cursor_ += 2;
        LexOctalNumber(start, leading_space, offset);
        return;
      }
      if (next == 'b' || next == 'B') {
        cursor_ += 2;
        LexBinaryNumber(start, leading_space, offset);
        return;
      }
    }

    // Decimal number.
    ScanDecimalDigits();

    bool is_float = false;

    // Check for fractional part: '.' followed by digit.
    if (cursor_ < end_ && *cursor_ == '.' && cursor_ + 1 < end_ &&
        IsDecimalDigit(*(cursor_ + 1))) {
      is_float = true;
      ++cursor_;  // skip '.'
      ScanDecimalDigits();
    }

    // Check for exponent.
    if (cursor_ < end_ && (*cursor_ == 'e' || *cursor_ == 'E')) {
      is_float = true;
      ++cursor_;
      if (cursor_ < end_ && (*cursor_ == '+' || *cursor_ == '-')) {
        ++cursor_;
      }
      ScanDecimalDigits();
    }

    llvm::StringRef text(start, cursor_ - start);

    if (is_float) {
      EmitFloatingLiteral(text, leading_space, offset);
    } else {
      EmitIntegerLiteral(text, leading_space, offset);
    }
  }

  void LexHexNumber(const char* start, bool leading_space, int32_t offset) {
    if (cursor_ >= end_ || !IsHexDigit(*cursor_)) {
      emitter_.Emit(start, ExpectedDigitAfterPrefix, std::string("0x"));
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    ScanHexDigits();

    bool is_float = false;

    // Hex float: '.' followed by hex digit.
    if (cursor_ < end_ && *cursor_ == '.' && cursor_ + 1 < end_ &&
        IsHexDigit(*(cursor_ + 1))) {
      is_float = true;
      ++cursor_;
      ScanHexDigits();
    }

    // Hex float requires 'p' or 'P' exponent.
    if (cursor_ < end_ && (*cursor_ == 'p' || *cursor_ == 'P')) {
      is_float = true;
      ++cursor_;
      if (cursor_ < end_ && (*cursor_ == '+' || *cursor_ == '-')) {
        ++cursor_;
      }
      ScanDecimalDigits();
    }

    llvm::StringRef text(start, cursor_ - start);

    if (is_float) {
      EmitFloatingLiteral(text, leading_space, offset);
    } else {
      EmitIntegerLiteral(text, leading_space, offset);
    }
  }

  void LexOctalNumber(const char* start, bool leading_space, int32_t offset) {
    if (cursor_ >= end_ || !(*cursor_ >= '0' && *cursor_ <= '7')) {
      emitter_.Emit(start, ExpectedDigitAfterPrefix, std::string("0o"));
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    while (cursor_ < end_ &&
           ((*cursor_ >= '0' && *cursor_ <= '7') || *cursor_ == '_')) {
      ++cursor_;
    }

    // Check for invalid digits.
    if (cursor_ < end_ && IsDecimalDigit(*cursor_)) {
      emitter_.Emit(start, InvalidNumberLiteral, std::string("octal"));
      while (cursor_ < end_ && (IsDecimalDigit(*cursor_) || *cursor_ == '_')) {
        ++cursor_;
      }
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    llvm::StringRef text(start, cursor_ - start);
    EmitIntegerLiteral(text, leading_space, offset);
  }

  void LexBinaryNumber(const char* start, bool leading_space, int32_t offset) {
    if (cursor_ >= end_ || !(*cursor_ == '0' || *cursor_ == '1')) {
      emitter_.Emit(start, ExpectedDigitAfterPrefix, std::string("0b"));
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    while (cursor_ < end_ &&
           (*cursor_ == '0' || *cursor_ == '1' || *cursor_ == '_')) {
      ++cursor_;
    }

    // Check for invalid digits.
    if (cursor_ < end_ && IsDecimalDigit(*cursor_)) {
      emitter_.Emit(start, InvalidNumberLiteral, std::string("binary"));
      while (cursor_ < end_ && (IsDecimalDigit(*cursor_) || *cursor_ == '_')) {
        ++cursor_;
      }
      buffer_.AddToken(
          TokenInfo(TokenKind::Error, leading_space,
                    /*error_length=*/static_cast<int>(cursor_ - start),
                    offset));
      buffer_.has_errors_ = true;
      return;
    }

    llvm::StringRef text(start, cursor_ - start);
    EmitIntegerLiteral(text, leading_space, offset);
  }

  void ScanDecimalDigits() {
    while (cursor_ < end_ && (IsDecimalDigit(*cursor_) || *cursor_ == '_')) {
      ++cursor_;
    }
  }

  void ScanHexDigits() {
    while (cursor_ < end_ && (IsHexDigit(*cursor_) || *cursor_ == '_')) {
      ++cursor_;
    }
  }

  void EmitIntegerLiteral(llvm::StringRef text, bool leading_space,
                          int32_t offset) {
    // Strip underscores for parsing.
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
      if (c != '_') clean.push_back(c);
    }

    // Determine the radix and strip the prefix.
    llvm::StringRef digits(clean);
    unsigned radix = 10;
    if (digits.starts_with("0x") || digits.starts_with("0X")) {
      digits = digits.drop_front(2);
      radix = 16;
    } else if (digits.starts_with("0o") || digits.starts_with("0O")) {
      digits = digits.drop_front(2);
      radix = 8;
    } else if (digits.starts_with("0b") || digits.starts_with("0B")) {
      digits = digits.drop_front(2);
      radix = 2;
    }

    // Parse into an APInt. Use enough bits to represent the value.
    if (digits.empty()) {
      // Fallback: store 0.
      auto int_id = value_stores_.ints().Add(static_cast<int64_t>(0));
      buffer_.AddToken(
          TokenInfo(TokenKind::IntegerLiteral, leading_space,
                    int_id.AsTokenPayload(), offset));
      return;
    }

    // Compute the number of bits needed.
    unsigned num_bits = llvm::APInt::getBitsNeeded(digits, radix);
    if (num_bits == 0) {
      num_bits = 1;
    }
    // Round up to at least 64 bits for consistency.
    if (num_bits < 64) {
      num_bits = 64;
    }

    llvm::APInt value(num_bits, digits, radix);
    auto int_id = value_stores_.ints().AddUnsigned(value);
    buffer_.AddToken(
        TokenInfo(TokenKind::IntegerLiteral, leading_space,
                  int_id.AsTokenPayload(), offset));
  }

  void EmitFloatingLiteral(llvm::StringRef text, bool leading_space,
                           int32_t offset) {
    // Parse the floating literal into mantissa/exponent form.
    // For now, store the raw text as a Real with simple parsing.
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
      if (c != '_') clean.push_back(c);
    }

    // Simple approach: store mantissa as the integer part, exponent as 0.
    // TODO: Parse properly into mantissa * 10^exponent form.
    Real real_value;
    real_value.mantissa = llvm::APInt(64, 0);
    real_value.exponent = llvm::APInt(64, 0);
    real_value.is_decimal = true;

    // Try to parse the decimal number properly.
    llvm::StringRef clean_ref(clean);
    bool is_hex = clean_ref.starts_with("0x") || clean_ref.starts_with("0X");

    if (is_hex) {
      // Hex float: parse mantissa and p-exponent.
      clean_ref = clean_ref.drop_front(2);
      auto dot_pos = clean_ref.find('.');
      auto p_pos = clean_ref.find_insensitive('p');

      llvm::StringRef mantissa_str;
      if (dot_pos != llvm::StringRef::npos) {
        // Combine integer and fractional hex digits.
        llvm::StringRef integer_part = clean_ref.substr(0, dot_pos);
        llvm::StringRef frac_part =
            (p_pos != llvm::StringRef::npos)
                ? clean_ref.slice(dot_pos + 1, p_pos)
                : clean_ref.substr(dot_pos + 1);
        std::string combined = (integer_part + frac_part).str();
        int frac_bits = frac_part.size() * 4;
        unsigned needed_bits =
            std::max(64u, static_cast<unsigned>(combined.size() * 4 + 1));
        real_value.mantissa = llvm::APInt(needed_bits, combined, 16);
        real_value.exponent =
            llvm::APInt(64, static_cast<uint64_t>(-frac_bits), true);
      } else {
        mantissa_str =
            (p_pos != llvm::StringRef::npos)
                ? clean_ref.substr(0, p_pos)
                : clean_ref;
        unsigned needed_bits =
            std::max(64u, static_cast<unsigned>(mantissa_str.size() * 4 + 1));
        real_value.mantissa = llvm::APInt(needed_bits, mantissa_str, 16);
        real_value.exponent = llvm::APInt(64, 0);
      }

      if (p_pos != llvm::StringRef::npos) {
        llvm::StringRef exp_str = clean_ref.substr(p_pos + 1);
        bool neg = false;
        if (exp_str.starts_with("-")) {
          neg = true;
          exp_str = exp_str.drop_front(1);
        } else if (exp_str.starts_with("+")) {
          exp_str = exp_str.drop_front(1);
        }
        uint64_t exp_val = 0;
        exp_str.getAsInteger(10, exp_val);
        int64_t signed_exp =
            real_value.exponent.getSExtValue() +
            (neg ? -static_cast<int64_t>(exp_val)
                 : static_cast<int64_t>(exp_val));
        real_value.exponent = llvm::APInt(64, signed_exp, true);
      }

      real_value.is_decimal = false;
    } else {
      // Decimal float.
      auto dot_pos = clean_ref.find('.');
      auto e_pos = clean_ref.find_insensitive('e');

      llvm::StringRef integer_part;
      llvm::StringRef frac_part;
      int frac_digits = 0;

      if (dot_pos != llvm::StringRef::npos) {
        integer_part = clean_ref.substr(0, dot_pos);
        frac_part = (e_pos != llvm::StringRef::npos)
                        ? clean_ref.slice(dot_pos + 1, e_pos)
                        : clean_ref.substr(dot_pos + 1);
        frac_digits = frac_part.size();
      } else {
        integer_part = (e_pos != llvm::StringRef::npos)
                           ? clean_ref.substr(0, e_pos)
                           : clean_ref;
      }

      std::string mantissa_str = (integer_part + frac_part).str();
      if (mantissa_str.empty()) mantissa_str = "0";
      unsigned needed_bits =
          std::max(64u, static_cast<unsigned>(mantissa_str.size() * 4 + 1));
      real_value.mantissa = llvm::APInt(needed_bits, mantissa_str, 10);
      int64_t exponent = -frac_digits;

      if (e_pos != llvm::StringRef::npos) {
        llvm::StringRef exp_str = clean_ref.substr(e_pos + 1);
        bool neg = false;
        if (exp_str.starts_with("-")) {
          neg = true;
          exp_str = exp_str.drop_front(1);
        } else if (exp_str.starts_with("+")) {
          exp_str = exp_str.drop_front(1);
        }
        uint64_t exp_val = 0;
        exp_str.getAsInteger(10, exp_val);
        exponent +=
            neg ? -static_cast<int64_t>(exp_val)
                : static_cast<int64_t>(exp_val);
      }

      real_value.exponent = llvm::APInt(64, exponent, true);
      real_value.is_decimal = true;
    }

    auto real_id = value_stores_.reals().Add(std::move(real_value));
    buffer_.AddToken(
        TokenInfo(TokenKind::FloatingLiteral, leading_space, real_id.index,
                  offset));
  }

  // === String Literal Lexing ===

  void LexStringLiteral(bool leading_space, int hash_count) {
    const char* start = cursor_;
    // If called from LexPound, start may include the # chars.
    // cursor_ is at the opening '"'.
    int32_t offset = static_cast<int32_t>(
        (start - hash_count) - source_text_.data());

    ++cursor_;  // skip opening '"'

    // Check for multiline """.
    if (cursor_ + 1 < end_ && *cursor_ == '"' && *(cursor_ + 1) == '"') {
      cursor_ += 2;
      LexMultilineString(leading_space, hash_count, offset);
      return;
    }

    // Single-line string.
    std::string value;
    while (cursor_ < end_) {
      char c = *cursor_;

      if (c == '"') {
        // Check for matching hash count.
        if (MatchClosingHashes(hash_count)) {
          auto str_id =
              value_stores_.string_literal_values().Add(llvm::StringRef(value));
          buffer_.AddToken(
              TokenInfo(TokenKind::StringLiteral, leading_space, str_id.index,
                        offset));
          return;
        }
        value.push_back(*cursor_);
        ++cursor_;
        continue;
      }

      if (c == '\\') {
        // Check for matching hash count for escape.
        const char* escape_start = cursor_;
        ++cursor_;
        bool has_matching_hashes = true;
        for (int i = 0; i < hash_count; ++i) {
          if (cursor_ >= end_ || *cursor_ != '#') {
            has_matching_hashes = false;
            break;
          }
          ++cursor_;
        }

        if (has_matching_hashes && cursor_ < end_ && *cursor_ == '(') {
          // String interpolation: for now, we don't break into segments.
          // Just scan to the matching ')' and include in the string value.
          // TODO: Implement proper string interpolation with segments.
          ++cursor_;
          int paren_depth = 1;
          while (cursor_ < end_ && paren_depth > 0) {
            if (*cursor_ == '(') ++paren_depth;
            else if (*cursor_ == ')') --paren_depth;
            if (paren_depth > 0) {
              value.push_back(*cursor_);
              ++cursor_;
            }
          }
          if (cursor_ < end_) ++cursor_;  // skip closing ')'
          continue;
        }

        // Regular escape: just consume the escape char.
        if (has_matching_hashes && cursor_ < end_) {
          value.push_back(ProcessEscapeChar(*cursor_));
          ++cursor_;
        } else {
          // Not a valid escape; backtrack and include backslash.
          cursor_ = escape_start;
          value.push_back(*cursor_);
          ++cursor_;
        }
        continue;
      }

      if (c == '\n' || c == '\r') {
        // Unterminated string.
        emitter_.Emit(start, UnterminatedString);
        buffer_.AddToken(
            TokenInfo(TokenKind::Error, leading_space,
                      /*error_length=*/static_cast<int>(cursor_ - start +
                                                        hash_count),
                      offset));
        buffer_.has_errors_ = true;
        return;
      }

      value.push_back(c);
      ++cursor_;
    }

    // Hit end of file.
    emitter_.Emit(start, UnterminatedString);
    buffer_.AddToken(
        TokenInfo(TokenKind::Error, leading_space,
                  /*error_length=*/static_cast<int>(cursor_ - start +
                                                    hash_count),
                  offset));
    buffer_.has_errors_ = true;
  }

  void LexMultilineString(bool leading_space, int hash_count, int32_t offset) {
    std::string value;

    while (cursor_ < end_) {
      if (*cursor_ == '"' && cursor_ + 2 < end_ && *(cursor_ + 1) == '"' &&
          *(cursor_ + 2) == '"') {
        cursor_ += 3;
        if (MatchClosingHashesOnly(hash_count)) {
          auto str_id =
              value_stores_.string_literal_values().Add(llvm::StringRef(value));
          buffer_.AddToken(
              TokenInfo(TokenKind::StringLiteral, leading_space, str_id.index,
                        offset));
          return;
        }
        // Not enough hashes; these quotes are part of the content.
        value += "\"\"\"";
        continue;
      }

      if (*cursor_ == '\n') {
        value.push_back('\n');
        ++cursor_;
        int32_t line_offset =
            static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.line_infos_.Add(LineInfo(line_offset));
        continue;
      }

      if (*cursor_ == '\r') {
        ++cursor_;
        if (cursor_ < end_ && *cursor_ == '\n') {
          ++cursor_;
        }
        value.push_back('\n');
        int32_t line_offset =
            static_cast<int32_t>(cursor_ - source_text_.data());
        buffer_.line_infos_.Add(LineInfo(line_offset));
        continue;
      }

      if (*cursor_ == '\\') {
        // Handle escapes in multiline strings similarly to single-line.
        const char* escape_start = cursor_;
        ++cursor_;
        bool has_matching_hashes = true;
        for (int i = 0; i < hash_count; ++i) {
          if (cursor_ >= end_ || *cursor_ != '#') {
            has_matching_hashes = false;
            break;
          }
          ++cursor_;
        }
        if (has_matching_hashes && cursor_ < end_) {
          value.push_back(ProcessEscapeChar(*cursor_));
          ++cursor_;
        } else {
          cursor_ = escape_start;
          value.push_back(*cursor_);
          ++cursor_;
        }
        continue;
      }

      value.push_back(*cursor_);
      ++cursor_;
    }

    // Unterminated multiline string.
    emitter_.Emit(source_text_.data() + offset, UnterminatedString);
    buffer_.AddToken(
        TokenInfo(TokenKind::Error, leading_space,
                  /*error_length=*/static_cast<int>(cursor_ -
                                                    (source_text_.data() +
                                                     offset)),
                  offset));
    buffer_.has_errors_ = true;
  }

  // After cursor_ is past the closing '"', check that the next `count`
  // characters are '#'. Advances cursor_ past them if so.
  auto MatchClosingHashes(int count) -> bool {
    ++cursor_;  // skip the '"'
    for (int i = 0; i < count; ++i) {
      if (cursor_ >= end_ || *cursor_ != '#') {
        // Not enough hashes. We need to backtrack past the quote.
        cursor_ -= (i + 1);
        return false;
      }
      ++cursor_;
    }
    return true;
  }

  // Like MatchClosingHashes but cursor_ is already past the quotes.
  auto MatchClosingHashesOnly(int count) -> bool {
    for (int i = 0; i < count; ++i) {
      if (cursor_ >= end_ || *cursor_ != '#') {
        cursor_ -= i;
        return false;
      }
      ++cursor_;
    }
    return true;
  }

  static auto ProcessEscapeChar(char c) -> char {
    switch (c) {
      case 'n': return '\n';
      case 't': return '\t';
      case 'r': return '\r';
      case '\\': return '\\';
      case '"': return '"';
      case '\'': return '\'';
      case '0': return '\0';
      default: return c;
    }
  }

  // === Operator & Punctuator Lexing ===

  void LexOperatorOrPunctuator(bool leading_space) {
    const char* start = cursor_;
    int32_t offset = static_cast<int32_t>(start - source_text_.data());
    char c = *cursor_;

    // First handle characters that can be either punctuators or operators.
    // We need to check context to decide.

    // ':' - either Colon or ColonColon symbol (not an operator).
    if (c == ':') {
      ++cursor_;
      if (cursor_ < end_ && *cursor_ == ':') {
        ++cursor_;
        buffer_.AddToken(TokenInfo(TokenKind::ColonColon, leading_space,
                                   offset));
      } else {
        buffer_.AddToken(TokenInfo(TokenKind::Colon, leading_space, offset));
      }
      return;
    }

    // '?' alone as postfix/infix - but can also start an operator sequence.
    // '!' alone as postfix - but can also start an operator sequence.
    // '.' can be Period or start of operator (..<, ...).
    // '=' can be Equal or start of == operator.
    // '-' can start -> or be an operator.

    // Check for specific multi-char symbol tokens first.
    if (c == '-' && cursor_ + 1 < end_ && *(cursor_ + 1) == '>') {
      cursor_ += 2;
      buffer_.AddToken(
          TokenInfo(TokenKind::MinusGreater, leading_space, offset));
      return;
    }

    if (c == '.' && cursor_ + 2 < end_ && *(cursor_ + 1) == '.' &&
        *(cursor_ + 2) == '.') {
      cursor_ += 3;
      buffer_.AddToken(
          TokenInfo(TokenKind::EllipsisToken, leading_space, offset));
      return;
    }

    // '=' alone is the assignment operator (a fixed symbol, not a user
    // operator).
    if (c == '=' && (cursor_ + 1 >= end_ ||
                     !IsOperatorContinuationChar(*(cursor_ + 1)) ||
                     *(cursor_ + 1) == '=')) {
      // Check for '==' first.
      if (cursor_ + 1 < end_ && *(cursor_ + 1) == '=') {
        // But '==' could be followed by more operator chars making it a
        // longer operator like '==='.
        if (cursor_ + 2 >= end_ ||
            !IsOperatorContinuationChar(*(cursor_ + 2))) {
          cursor_ += 2;
          buffer_.AddToken(
              TokenInfo(TokenKind::EqualEqual, leading_space, offset));
          return;
        }
      } else if (cursor_ + 1 >= end_ ||
                 !IsOperatorContinuationChar(*(cursor_ + 1))) {
        ++cursor_;
        buffer_.AddToken(TokenInfo(TokenKind::Equal, leading_space, offset));
        return;
      }
    }

    // Now handle general operator scanning.
    // Scan all consecutive operator characters.
    while (cursor_ < end_ && IsOperatorContinuationChar(*cursor_)) {
      // Stop before '//' or '/*' (comment start).
      if (*cursor_ == '/' && cursor_ + 1 < end_) {
        if (*(cursor_ + 1) == '/' || *(cursor_ + 1) == '*') {
          break;
        }
      }
      ++cursor_;
    }

    llvm::StringRef op_text(start, cursor_ - start);

    // Check for fixed multi-char symbols that got absorbed into operator scan.
    if (op_text == "==") {
      buffer_.AddToken(
          TokenInfo(TokenKind::EqualEqual, leading_space, offset));
      return;
    }
    if (op_text == "!=") {
      buffer_.AddToken(
          TokenInfo(TokenKind::ExclaimEqual, leading_space, offset));
      return;
    }
    if (op_text == ">=") {
      buffer_.AddToken(
          TokenInfo(TokenKind::GreaterEqual, leading_space, offset));
      return;
    }
    if (op_text == "<=") {
      buffer_.AddToken(
          TokenInfo(TokenKind::LessEqual, leading_space, offset));
      return;
    }
    if (op_text == "->") {
      buffer_.AddToken(
          TokenInfo(TokenKind::MinusGreater, leading_space, offset));
      return;
    }
    if (op_text == "...") {
      buffer_.AddToken(
          TokenInfo(TokenKind::EllipsisToken, leading_space, offset));
      return;
    }

    // Single-char punctuators that shouldn't become operators in certain
    // contexts.
    if (op_text.size() == 1) {
      switch (c) {
        case '.':
          buffer_.AddToken(
              TokenInfo(TokenKind::Period, leading_space, offset));
          return;
        case '=':
          buffer_.AddToken(
              TokenInfo(TokenKind::Equal, leading_space, offset));
          return;
        case '&':
          // Lone '&' is the AmpPrefix symbol in Swift.
          buffer_.AddToken(
              TokenInfo(TokenKind::Amp, leading_space, offset));
          return;
        case '!':
          buffer_.AddToken(
              TokenInfo(TokenKind::Exclaim, leading_space, offset));
          return;
        default:
          break;
      }
    }

    // Classify the operator by spacing context.
    bool left_bound = IsLeftBound(start);
    bool right_bound = IsRightBound(cursor_);

    TokenKind kind;
    if (!left_bound && !right_bound) {
      kind = TokenKind::OperatorBinarySpaced;
    } else if (left_bound && right_bound) {
      kind = TokenKind::OperatorBinaryUnspaced;
    } else if (!left_bound && right_bound) {
      kind = TokenKind::OperatorPrefix;
    } else {
      kind = TokenKind::OperatorPostfix;
    }

    auto id = value_stores_.identifiers().Add(op_text);
    buffer_.AddToken(TokenInfo(kind, leading_space, id.index, offset));
  }

  auto IsLeftBound(const char* pos) -> bool {
    if (pos <= source_text_.data()) return false;
    char prev = *(pos - 1);
    return IsIdentifierContinuation(prev) || prev == ')' || prev == ']' ||
           prev == '}' || prev == '!' || prev == '?' || prev == '.' ||
           prev == '"' || prev == '\'';
  }

  auto IsRightBound(const char* pos) -> bool {
    if (pos >= end_) return false;
    char next = *pos;
    return IsIdentifierStart(next) || next == '(' || next == '[' ||
           next == '{' || next == '.' || next == '"' || next == '\'' ||
           next == '$';
  }

  // === Pound Keyword Lexing ===

  void LexPound(bool leading_space) {
    const char* start = cursor_;
    int32_t offset = static_cast<int32_t>(start - source_text_.data());
    ++cursor_;  // skip '#'

    // Count consecutive '#' for raw string delimiters.
    int hash_count = 1;
    while (cursor_ < end_ && *cursor_ == '#') {
      ++hash_count;
      ++cursor_;
    }

    // Check for raw string literal: ##..."..."##
    if (cursor_ < end_ && *cursor_ == '"') {
      LexStringLiteral(leading_space, hash_count);
      return;
    }

    // If we consumed more than one '#', and it's not a string literal,
    // emit the '#' tokens individually. Backtrack.
    if (hash_count > 1) {
      cursor_ = start + 1;
      buffer_.AddToken(TokenInfo(TokenKind::Pound, leading_space, offset));
      return;
    }

    // Try to match a pound keyword: #identifier
    if (cursor_ < end_ && IsIdentifierStart(*cursor_)) {
      const char* ident_start = cursor_;
      while (cursor_ < end_ && IsIdentifierContinuation(*cursor_)) {
        ++cursor_;
      }
      llvm::StringRef full_text(start, cursor_ - start);

      TokenKind kind = LookupPoundKeyword(full_text);
      if (kind != TokenKind::Error) {
        buffer_.AddToken(TokenInfo(kind, leading_space, offset));
        return;
      }

      // Not a known pound keyword. Backtrack and emit just '#'.
      cursor_ = ident_start;
    }

    buffer_.AddToken(TokenInfo(TokenKind::Pound, leading_space, offset));
  }

  static auto LookupPoundKeyword(llvm::StringRef text) -> TokenKind {
    return llvm::StringSwitch<TokenKind>(text)
        .Case("#keyPath", TokenKind::PoundKeyPath)
        .Case("#line", TokenKind::PoundLine)
        .Case("#selector", TokenKind::PoundSelector)
        .Case("#file", TokenKind::PoundFile)
        .Case("#fileID", TokenKind::PoundFileID)
        .Case("#filePath", TokenKind::PoundFilePath)
        .Case("#column", TokenKind::PoundColumn)
        .Case("#function", TokenKind::PoundFunction)
        .Case("#dsohandle", TokenKind::PoundDsohandle)
        .Case("#assert", TokenKind::PoundAssert)
        .Case("#sourceLocation", TokenKind::PoundSourceLocation)
        .Case("#warning", TokenKind::PoundWarning)
        .Case("#error", TokenKind::PoundError)
        .Case("#if", TokenKind::PoundIf)
        .Case("#else", TokenKind::PoundElse)
        .Case("#elseif", TokenKind::PoundElseif)
        .Case("#endif", TokenKind::PoundEndif)
        .Case("#available", TokenKind::PoundAvailable)
        .Case("#unavailable", TokenKind::PoundUnavailable)
        .Case("#fileLiteral", TokenKind::PoundFileLiteral)
        .Case("#imageLiteral", TokenKind::PoundImageLiteral)
        .Case("#colorLiteral", TokenKind::PoundColorLiteral)
        .Case("#_hasSymbol", TokenKind::PoundHasSymbol)
        .Default(TokenKind::Error);
  }

  // === Bracket Matching ===

  void MatchOpenCloseTokens() {
    struct OpenEntry {
      TokenIndex index;
      TokenKind kind;
    };
    llvm::SmallVector<OpenEntry, 16> open_stack;

    for (TokenIndex token : buffer_.tokens()) {
      TokenKind kind = buffer_.GetKind(token);

      if (kind.is_opening_symbol()) {
        open_stack.push_back({token, kind});
        continue;
      }

      if (kind.is_closing_symbol()) {
        TokenKind expected_opening = kind.opening_symbol();

        if (!open_stack.empty() &&
            open_stack.back().kind == expected_opening) {
          // Match.
          TokenIndex opening = open_stack.back().index;
          open_stack.pop_back();

          buffer_.token_infos_.Get(opening).set_closing_token_index(token);
          buffer_.token_infos_.Get(token).set_opening_token_index(opening);
        } else {
          // Unmatched closing.
          emitter_.Emit(
              source_text_.data() +
                  buffer_.token_infos_.Get(token).byte_offset(),
              UnmatchedClosing,
              std::string(kind.fixed_spelling()));
          buffer_.has_errors_ = true;
          // Set self-referencing match for error recovery.
          buffer_.token_infos_.Get(token).set_opening_token_index(token);

          // Mark as recovery token.
          if (buffer_.recovery_tokens_.empty()) {
            buffer_.recovery_tokens_.resize(buffer_.token_infos_.size());
          }
          buffer_.recovery_tokens_.set(token.index);
        }
      }
    }

    // Report unmatched openings.
    for (auto& entry : open_stack) {
      emitter_.Emit(
          source_text_.data() +
              buffer_.token_infos_.Get(entry.index).byte_offset(),
          UnmatchedOpening,
          std::string(entry.kind.fixed_spelling()));
      buffer_.has_errors_ = true;

      // Create a recovery closing token.
      int32_t eof_offset = static_cast<int32_t>(source_text_.size());
      TokenKind closing_kind = entry.kind.closing_symbol();
      auto closing_index = buffer_.AddToken(
          TokenInfo(closing_kind, /*has_leading_space=*/false, eof_offset));

      buffer_.token_infos_.Get(entry.index)
          .set_closing_token_index(closing_index);
      buffer_.token_infos_.Get(closing_index)
          .set_opening_token_index(entry.index);

      if (buffer_.recovery_tokens_.empty()) {
        buffer_.recovery_tokens_.resize(buffer_.token_infos_.size());
      }
      buffer_.recovery_tokens_.set(closing_index.index);
    }
  }

  // === Helper ===

  void EmitOneCharSymbol(TokenKind kind, bool leading_space) {
    int32_t offset = static_cast<int32_t>(cursor_ - source_text_.data());
    buffer_.AddToken(TokenInfo(kind, leading_space, offset));
    ++cursor_;
  }

  // === Fields ===

  TokenizedBuffer buffer_;
  SharedValueStores& value_stores_;
  llvm::StringRef source_text_;
  const char* cursor_;
  const char* end_;
  TokenizedBuffer::SourcePointerDiagnosticEmitter emitter_;
  LexOptions options_;

  bool has_leading_space_ = false;
  bool at_start_of_line_ = true;
};

auto Lex(SharedValueStores& value_stores, SourceBuffer& source,
         LexOptions options) -> TokenizedBuffer {
  auto* consumer =
      options.consumer ? options.consumer : &Diagnostics::ConsoleConsumer();

  Lexer lexer(value_stores, source, consumer, options);
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
