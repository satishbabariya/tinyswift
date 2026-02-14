// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LEX_CHARACTER_SET_H_
#define TINYSWIFT_TOOLCHAIN_LEX_CHARACTER_SET_H_

#include "llvm/ADT/StringExtras.h"

namespace TinySwift::Lex {

// Is this an alphabetical character according to TinySwift's lexical rules?
//
// Alphabetical characters are permitted at the start of identifiers. This
// currently includes 'A'..'Z' and 'a'..'z'.
inline auto IsAlpha(char c) -> bool { return llvm::isAlpha(c); }

// Is this a decimal digit according to TinySwift's lexical rules?
//
// This currently includes '0'..'9'.
inline auto IsDecimalDigit(char c) -> bool { return llvm::isDigit(c); }

// Is this an alphanumeric character according to TinySwift's lexical rules?
//
// Alphanumeric characters are permitted as trailing characters in identifiers
// and numeric literals. This includes alphabetical characters plus decimal
// digits.
//
// Note that '_' is not considered alphanumeric, despite in most circumstances
// being a valid continuation character of an identifier or numeric literal.
inline auto IsAlnum(char c) -> bool { return llvm::isAlnum(c); }

// Is this a hexadecimal digit?
inline auto IsHexDigit(char c) -> bool { return llvm::isHexDigit(c); }

// Is this an uppercase hexadecimal digit?
//
// Note that lowercase 'a'..'f' are currently not considered in some contexts.
inline auto IsUpperHexDigit(char c) -> bool {
  return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F');
}

// Is this a lowercase letter?
inline auto IsLower(char c) -> bool { return 'a' <= c && c <= 'z'; }

// Is this character a valid start of a Swift identifier?
//
// Swift identifiers start with a letter or underscore. Unicode identifiers
// are deferred to a later phase; currently ASCII-only.
inline auto IsIdentifierStart(char c) -> bool {
  return llvm::isAlpha(c) || c == '_';
}

// Is this character a valid continuation of a Swift identifier?
//
// Swift identifier continuation includes letters, digits, and underscore.
inline auto IsIdentifierContinuation(char c) -> bool {
  return llvm::isAlnum(c) || c == '_';
}

// Is this character a valid start of a Swift operator?
//
// Swift operators are composed of the characters: / = - + * % < > ! & | ^ ~ .
// Note: these are NOT fixed punctuators in Swift; they form user-definable
// operators classified by spacing context.
inline auto IsOperatorStartChar(char c) -> bool {
  switch (c) {
    case '/': case '=': case '-': case '+': case '*': case '%':
    case '<': case '>': case '!': case '&': case '|': case '^':
    case '~': case '.':
      return true;
    default:
      return false;
  }
}

// Is this character a valid continuation of a Swift operator?
//
// Currently the same as IsOperatorStartChar for ASCII. Unicode combining
// marks would be added in a later phase.
inline auto IsOperatorContinuationChar(char c) -> bool {
  return IsOperatorStartChar(c);
}

// Is this character considered to be horizontal whitespace?
//
// Such characters can appear in the indentation of a line.
inline auto IsHorizontalWhitespace(char c) -> bool {
  return c == ' ' || c == '\t';
}

// Is this character considered to be vertical whitespace?
//
// Such characters are considered to terminate lines. Includes both \n and \r
// for CR/LF handling.
inline auto IsVerticalWhitespace(char c) -> bool {
  return c == '\n' || c == '\r';
}

// Is this character considered to be whitespace?
//
// Changes here will need matching changes in
// `TokenizedBuffer::Lexer::SkipWhitespace`.
inline auto IsSpace(char c) -> bool {
  return IsHorizontalWhitespace(c) || IsVerticalWhitespace(c);
}

}  // namespace TinySwift::Lex

#endif  // TINYSWIFT_TOOLCHAIN_LEX_CHARACTER_SET_H_
