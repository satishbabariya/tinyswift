// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/lex/token_kind.h"

namespace TinySwift::Lex {

TINYSWIFT_DEFINE_ENUM_CLASS_NAMES(TokenKind) {
#define TINYSWIFT_TOKEN(TokenName) TINYSWIFT_ENUM_CLASS_NAME_STRING(TokenName)
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_SYMBOL_TOKEN(TokenName, Spelling) true,
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsGroupingSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_OPENING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, ClosingName) \
  true,
#define TINYSWIFT_CLOSING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, OpeningName) \
  true,
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsOpeningSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_OPENING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, ClosingName) \
  true,
#include "toolchain/lex/token_kind.def"
};

constexpr TokenKind TokenKind::ClosingSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) Error,
#define TINYSWIFT_OPENING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, ClosingName) \
  ClosingName,
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsClosingSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_CLOSING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, OpeningName) \
  true,
#include "toolchain/lex/token_kind.def"
};

constexpr TokenKind TokenKind::OpeningSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) Error,
#define TINYSWIFT_CLOSING_GROUP_SYMBOL_TOKEN(TokenName, Spelling, OpeningName) \
  OpeningName,
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsOneCharSymbol[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_ONE_CHAR_SYMBOL_TOKEN(TokenName, Spelling) true,
#include "toolchain/lex/token_kind.def"
};

constexpr bool TokenKind::IsKeyword[] = {
#define TINYSWIFT_TOKEN(TokenName) false,
#define TINYSWIFT_KEYWORD_TOKEN(TokenName, Spelling) true,
#include "toolchain/lex/token_kind.def"
};

constexpr llvm::StringLiteral TokenKind::FixedSpelling[] = {
#define TINYSWIFT_TOKEN(TokenName) "",
#define TINYSWIFT_SYMBOL_TOKEN(TokenName, Spelling) Spelling,
#define TINYSWIFT_KEYWORD_TOKEN(TokenName, Spelling) Spelling,
#include "toolchain/lex/token_kind.def"
};

constexpr int8_t TokenKind::ExpectedParseTreeSize[] = {
#define TINYSWIFT_TOKEN(Name) 1,
#define TINYSWIFT_TOKEN_WITH_VIRTUAL_NODE(size) 2,
#include "toolchain/lex/token_kind.def"
};

}  // namespace TinySwift::Lex
