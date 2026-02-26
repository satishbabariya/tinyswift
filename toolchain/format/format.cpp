// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/format/format.h"

#include "toolchain/lex/token_kind.h"

namespace TinySwift::Format {

namespace {

using Lex::TokenKind;

// Returns true if the token kind is a declaration-introducing keyword that
// should have a blank line before it at the top level.
static auto IsDeclIntroducer(TokenKind kind) -> bool {
  return kind == TokenKind::FuncKeyword ||
         kind == TokenKind::StructKeyword ||
         kind == TokenKind::ClassKeyword ||
         kind == TokenKind::EnumKeyword ||
         kind == TokenKind::ProtocolKeyword ||
         kind == TokenKind::ExtensionKeyword ||
         kind == TokenKind::ImportKeyword ||
         kind == TokenKind::TypealiasKeyword;
}

// Returns true if the token kind is a keyword that should be followed by a
// space (when the next token is not a grouping symbol that attaches directly).
static auto IsSpaceAfterKeyword(TokenKind kind) -> bool {
  // Most keywords want a space after them. The exceptions are keywords that
  // act as values (true, false, nil, self, super, Self) or postfix-like.
  return kind.is_keyword() &&
         kind != TokenKind::TrueKeyword &&
         kind != TokenKind::FalseKeyword &&
         kind != TokenKind::NilKeyword &&
         kind != TokenKind::SelfKeyword &&
         kind != TokenKind::SuperKeyword &&
         kind != TokenKind::CapitalSelfKeyword;
}

// Returns true if this token kind is a binary operator symbol (fixed-spelling
// two-character operators like ==, !=, ->, etc.) that should have spaces
// around them.
static auto IsBinaryOperatorSymbol(TokenKind kind) -> bool {
  return kind == TokenKind::Equal ||
         kind == TokenKind::EqualEqual ||
         kind == TokenKind::ExclaimEqual ||
         kind == TokenKind::LessEqual ||
         kind == TokenKind::GreaterEqual ||
         kind == TokenKind::MinusGreater ||
         kind == TokenKind::QuestionQuestion;
}

// Returns true if an operator text represents a common binary operator that
// should always have spaces around it, even when the lexer classifies it as
// prefix or postfix due to adjacent spacing in the source.
static auto IsBinaryLikeOperatorText(llvm::StringRef text) -> bool {
  return text == "+" || text == "-" || text == "*" || text == "/" ||
         text == "%" || text == "<" || text == ">" || text == "&&" ||
         text == "||" || text == "&+" || text == "&-" || text == "&*" ||
         text == "<<" || text == ">>" || text == "^" || text == "|" ||
         text == "&" || text == "~=";
}

// Emits appropriate spacing between two adjacent tokens.
static auto EmitSpacing(TokenKind prev, TokenKind current,
                        llvm::StringRef prev_text, llvm::StringRef current_text,
                        llvm::raw_ostream& out) -> void {
  // No space after opening grouping symbols: (, [
  if (prev == TokenKind::OpenParen || prev == TokenKind::OpenSquareBracket) {
    return;
  }

  // No space before closing grouping symbols: ), ]
  if (current == TokenKind::CloseParen ||
      current == TokenKind::CloseSquareBracket) {
    return;
  }

  // No space before comma or semicolon.
  if (current == TokenKind::Comma || current == TokenKind::Semi) {
    return;
  }

  // Period (dot): no space on either side for member access. Allow space
  // before period only after specific keywords where the dot introduces a
  // pattern or enum case (e.g., `case .north`, `return .value`).
  if (current == TokenKind::Period) {
    if (prev == TokenKind::CaseKeyword ||
        prev == TokenKind::ReturnKeyword ||
        prev == TokenKind::IsKeyword ||
        prev == TokenKind::AsKeyword ||
        prev == TokenKind::LetKeyword ||
        prev == TokenKind::VarKeyword ||
        prev == TokenKind::Equal ||
        prev == TokenKind::Comma ||
        prev == TokenKind::Colon ||
        prev == TokenKind::OpenParen ||
        prev == TokenKind::OpenSquareBracket) {
      out << ' ';
      return;
    }
    return;
  }
  if (prev == TokenKind::Period) {
    return;
  }

  // Operators classified as prefix/postfix but with binary-like spellings
  // (e.g., > in `x>0`) should get spaces. Check this before the general
  // prefix/postfix suppression rules.
  if ((prev == TokenKind::OperatorPostfix &&
       IsBinaryLikeOperatorText(prev_text)) ||
      (current == TokenKind::OperatorPrefix &&
       IsBinaryLikeOperatorText(current_text)) ||
      (prev == TokenKind::OperatorPrefix &&
       IsBinaryLikeOperatorText(prev_text)) ||
      (current == TokenKind::OperatorPostfix &&
       IsBinaryLikeOperatorText(current_text))) {
    out << ' ';
    return;
  }

  // No space after prefix operators (!, &, prefix custom operators).
  if (prev == TokenKind::OperatorPrefix ||
      prev == TokenKind::Exclaim ||
      prev == TokenKind::Amp) {
    return;
  }

  // No space before postfix operators (!, ?, postfix custom operators).
  if (current == TokenKind::OperatorPostfix ||
      current == TokenKind::Question ||
      current == TokenKind::Exclaim) {
    return;
  }

  // No space after @ (for attributes like @extern).
  if (prev == TokenKind::At) {
    return;
  }

  // No space after # (for pound keywords).
  if (prev == TokenKind::Pound) {
    return;
  }

  // No space after backslash (for keypaths).
  if (prev == TokenKind::Backslash) {
    return;
  }

  // No space before colon in most contexts (parameter labels, dict literals).
  // Colon gets a space after it though (handled below as default).
  if (current == TokenKind::Colon) {
    // Space before colon only in ternary / switch-case context is hard to
    // determine at the token level, so we use no-space-before as default.
    return;
  }

  // No space around :: (scope resolution).
  if (prev == TokenKind::ColonColon || current == TokenKind::ColonColon) {
    return;
  }

  // Ellipsis: no space around it in range expressions.
  if (prev == TokenKind::EllipsisToken || current == TokenKind::EllipsisToken) {
    return;
  }

  // Space after comma.
  if (prev == TokenKind::Comma) {
    out << ' ';
    return;
  }

  // Space after colon.
  if (prev == TokenKind::Colon) {
    out << ' ';
    return;
  }

  // Space around binary operator symbols (=, ==, !=, ->, >=, <=, ??).
  if (IsBinaryOperatorSymbol(prev) || IsBinaryOperatorSymbol(current)) {
    out << ' ';
    return;
  }

  // Space around binary operators (+, -, *, /, <, >, &&, || etc.).
  // The formatter normalizes both spaced and unspaced binary operators to
  // have spaces around them.
  if (prev == TokenKind::OperatorBinarySpaced ||
      current == TokenKind::OperatorBinarySpaced ||
      prev == TokenKind::OperatorBinaryUnspaced ||
      current == TokenKind::OperatorBinaryUnspaced) {
    out << ' ';
    return;
  }

  // Space before open curly brace.
  if (current == TokenKind::OpenCurlyBrace) {
    out << ' ';
    return;
  }

  // Space after keywords that take arguments — but not before `(` for keywords
  // that directly take parenthesized arguments (init, subscript, deinit).
  if (IsSpaceAfterKeyword(prev)) {
    if (current == TokenKind::OpenParen &&
        (prev == TokenKind::InitKeyword ||
         prev == TokenKind::SubscriptKeyword)) {
      return;
    }
    out << ' ';
    return;
  }

  // Space after close paren/bracket before non-punctuation (e.g. `-> Int`).
  if ((prev == TokenKind::CloseParen ||
       prev == TokenKind::CloseSquareBracket ||
       prev == TokenKind::CloseCurlyBrace) &&
      !current.is_symbol() && !current.is_closing_symbol()) {
    out << ' ';
    return;
  }

  // Space between identifier-like tokens (identifiers, literals, keywords
  // used as values).
  bool prev_is_word = prev == TokenKind::Identifier ||
                      prev == TokenKind::DollarIdent ||
                      prev == TokenKind::EscapedIdentifier ||
                      prev.is_literal() ||
                      prev == TokenKind::TrueKeyword ||
                      prev == TokenKind::FalseKeyword ||
                      prev == TokenKind::NilKeyword ||
                      prev == TokenKind::SelfKeyword ||
                      prev == TokenKind::SuperKeyword ||
                      prev == TokenKind::CapitalSelfKeyword ||
                      prev == TokenKind::AnyKeyword;
  bool current_is_word = current == TokenKind::Identifier ||
                         current == TokenKind::DollarIdent ||
                         current == TokenKind::EscapedIdentifier ||
                         current.is_literal() ||
                         current.is_keyword();
  if (prev_is_word && current_is_word) {
    out << ' ';
    return;
  }

  // Space after close paren before open paren (chained calls are fine without,
  // but things like `func foo() throws` need space). Use space as default
  // between close and open parens only when a keyword intervenes — but that's
  // handled above. For direct `)(`, no space.
  if (prev == TokenKind::CloseParen && current == TokenKind::OpenParen) {
    return;
  }

  // Space after identifier/literal before open paren — NO space. Function
  // calls: foo(x).
  if (prev_is_word && current == TokenKind::OpenParen) {
    return;
  }

  // Space after identifier before open square bracket — NO space. Subscript:
  // arr[0].
  if (prev_is_word && current == TokenKind::OpenSquareBracket) {
    return;
  }

  // Default: space between tokens that don't have a specific rule.
  // This handles cases like `throws ->` and similar.
  if (prev_is_word || current_is_word) {
    out << ' ';
    return;
  }

  // For remaining symbol-to-symbol cases, use a space as a safe default.
  out << ' ';
}

}  // namespace

auto Format(const Lex::TokenizedBuffer& tokens, llvm::raw_ostream& out)
    -> bool {
  int indent_level = 0;
  bool at_line_start = true;
  TokenKind prev_kind = TokenKind::FileStart;
  llvm::StringRef prev_text;
  bool prev_was_close_brace = false;
  int prev_line = -1;

  for (Lex::TokenIndex token : tokens.tokens()) {
    TokenKind kind = tokens.GetKind(token);
    llvm::StringRef text = tokens.GetTokenText(token);
    int current_line = tokens.GetLineNumber(token);

    // Skip boundary tokens.
    if (kind == TokenKind::FileStart || kind == TokenKind::FileEnd) {
      continue;
    }

    // Skip error/recovery tokens.
    if (kind == TokenKind::Error) {
      continue;
    }

    // Track whether the previous token was a close brace (for blank line
    // insertion between top-level declarations).
    bool this_is_close_brace = (kind == TokenKind::CloseCurlyBrace);

    // Decrease indent BEFORE emitting close brace.
    if (kind == TokenKind::CloseCurlyBrace) {
      if (indent_level > 0) {
        indent_level--;
      }
    }

    // Emit newline before } if not already at line start.
    if (kind == TokenKind::CloseCurlyBrace && !at_line_start) {
      out << '\n';
      at_line_start = true;
    }

    // If the source had a line break between the previous token and this one,
    // and we haven't already emitted a newline (e.g., after { or }), insert
    // one now. This preserves statement separation from the source.
    if (!at_line_start && prev_line >= 0 && current_line > prev_line &&
        kind != TokenKind::CloseCurlyBrace) {
      out << '\n';
      at_line_start = true;
    }

    // Insert blank line between top-level declarations.
    if (indent_level == 0 && IsDeclIntroducer(kind) && prev_was_close_brace) {
      // We're already at line start after the close brace's newline, so
      // just emit one more newline for the blank line.
      out << '\n';
    }

    // Emit indentation or inter-token spacing.
    if (at_line_start) {
      for (int i = 0; i < indent_level * 2; ++i) {
        out << ' ';
      }
      at_line_start = false;
    } else {
      EmitSpacing(prev_kind, kind, prev_text, text, out);
    }

    // Emit the token text.
    out << text;

    // Post-token: handle newlines after { and ;
    if (kind == TokenKind::OpenCurlyBrace) {
      indent_level++;
      out << '\n';
      at_line_start = true;
    } else if (kind == TokenKind::Semi) {
      out << '\n';
      at_line_start = true;
    }

    // Track state for next iteration.
    prev_was_close_brace = this_is_close_brace;
    prev_kind = kind;
    prev_text = text;
    prev_line = current_line;
  }

  // Ensure the file ends with a newline.
  if (!at_line_start) {
    out << '\n';
  }

  return true;
}

}  // namespace TinySwift::Format
