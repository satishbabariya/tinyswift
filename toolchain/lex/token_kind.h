// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LEX_TOKEN_KIND_H_
#define TINYSWIFT_TOOLCHAIN_LEX_TOKEN_KIND_H_

#include <cstdint>

#include "common/check.h"
#include "common/enum_base.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadicDetails.h"

namespace TinySwift::Lex {

TINYSWIFT_DEFINE_RAW_ENUM_CLASS(TokenKind, uint8_t) {
#define TINYSWIFT_TOKEN(TokenName) TINYSWIFT_RAW_ENUM_ENUMERATOR(TokenName)
#include "toolchain/lex/token_kind.def"
};

class TokenKind : public TINYSWIFT_ENUM_BASE(TokenKind) {
 public:
#define TINYSWIFT_TOKEN(TokenName) TINYSWIFT_ENUM_CONSTANT_DECL(TokenName)
#include "toolchain/lex/token_kind.def"

  // An array of all the keyword tokens.
  static const llvm::ArrayRef<TokenKind> KeywordTokens;

  // An array of all the pound keyword tokens.
  static const llvm::ArrayRef<TokenKind> PoundKeywordTokens;

  using EnumBase::EnumBase;

  // Permit creation from RawEnumType for templates.
  using EnumBase::Make;

  // Permit conversion to integer for use as an array index.
  using EnumBase::AsInt;

  // Test whether this kind of token is a simple symbol sequence (punctuation,
  // not letters) that appears directly in the source text and can be
  // unambiguously lexed with `starts_with` logic. While these may appear
  // inside of other tokens, outside of the contents of other tokens they
  // don't require any specific characters before or after to distinguish them
  // in the source. Returns false otherwise.
  auto is_symbol() const -> bool { return kIsSymbol[AsInt()]; }

  // Test whether this kind of token is a grouping symbol (part of an opening
  // and closing pair that must always be matched in the token stream).
  auto is_grouping_symbol() const -> bool { return kIsGroupingSymbol[AsInt()]; }

  // Test whether this kind of token is an opening symbol for a group.
  auto is_opening_symbol() const -> bool { return kIsOpeningSymbol[AsInt()]; }

  // Returns the associated closing symbol for an opening symbol.
  //
  // The token kind must be an opening symbol.
  auto closing_symbol() const -> TokenKind {
    auto result = kClosingSymbol[AsInt()];
    TINYSWIFT_DCHECK(result != Error, "Only opening symbols are valid!");
    return result;
  }

  // Test whether this kind of token is a closing symbol for a group.
  auto is_closing_symbol() const -> bool { return kIsClosingSymbol[AsInt()]; }

  // Returns the associated opening symbol for a closing symbol.
  //
  // The token kind must be a closing symbol.
  auto opening_symbol() const -> TokenKind {
    auto result = kOpeningSymbol[AsInt()];
    TINYSWIFT_DCHECK(result != Error, "Only closing symbols are valid!");
    return result;
  }

  // Test whether this kind of token is a one-character symbol whose character
  // is not part of any other symbol.
  auto is_one_char_symbol() const -> bool { return kIsOneCharSymbol[AsInt()]; }

  // Test whether this kind of token is a keyword.
  auto is_keyword() const -> bool { return kIsKeyword[AsInt()]; }

  // Test whether this kind of token is a statement keyword.
  auto is_stmt_keyword() const -> bool { return kIsStmtKeyword[AsInt()]; }

  // Test whether this kind of token is an expression keyword.
  auto is_expr_keyword() const -> bool { return kIsExprKeyword[AsInt()]; }

  // Test whether this kind of token is a pound keyword.
  auto is_pound_keyword() const -> bool { return kIsPoundKeyword[AsInt()]; }

  // Test whether this kind of token is an operator.
  auto is_operator() const -> bool {
    return *this == OperatorBinarySpaced ||
           *this == OperatorBinaryUnspaced ||
           *this == OperatorPrefix ||
           *this == OperatorPostfix;
  }

  // Test whether this kind of token is a literal.
  auto is_literal() const -> bool {
    return *this == IntegerLiteral ||
           *this == FloatingLiteral ||
           *this == StringLiteral;
  }

  // If this token kind has a fixed spelling when in source code, returns it.
  // Otherwise returns an empty string.
  auto fixed_spelling() const -> llvm::StringLiteral {
    return kFixedSpelling[AsInt()];
  }

  // Get the expected number of parse tree nodes that will be created for this
  // token.
  auto expected_max_parse_tree_size() const -> int {
    return kExpectedParseTreeSize[AsInt()];
  }

  // Test whether this token kind is in the provided list.
  auto IsOneOf(std::initializer_list<TokenKind> kinds) const -> bool {
    for (TokenKind kind : kinds) {
      if (*this == kind) {
        return true;
      }
    }
    return false;
  }

 private:
  static const TokenKind KeywordTokensStorage[];
  static const TokenKind PoundKeywordTokensStorage[];

  static const bool kIsSymbol[];
  static const bool kIsGroupingSymbol[];
  static const bool kIsOpeningSymbol[];
  static const TokenKind kClosingSymbol[];
  static const bool kIsClosingSymbol[];
  static const TokenKind kOpeningSymbol[];
  static const bool kIsOneCharSymbol[];

  static const bool kIsKeyword[];
  static const bool kIsStmtKeyword[];
  static const bool kIsExprKeyword[];
  static const bool kIsPoundKeyword[];

  static const llvm::StringLiteral kFixedSpelling[];

  static const int8_t kExpectedParseTreeSize[];
};

#define TINYSWIFT_TOKEN(TokenName) \
  TINYSWIFT_ENUM_CONSTANT_DEFINITION(TokenKind, TokenName)
#include "toolchain/lex/token_kind.def"

inline constexpr TokenKind TokenKind::KeywordTokensStorage[] = {
#define TINYSWIFT_KEYWORD_TOKEN(TokenName, Spelling) TokenKind::TokenName,
#include "toolchain/lex/token_kind.def"
};
inline constexpr llvm::ArrayRef<TokenKind> TokenKind::KeywordTokens =
    KeywordTokensStorage;

inline constexpr TokenKind TokenKind::PoundKeywordTokensStorage[] = {
#define TINYSWIFT_POUND_KEYWORD_TOKEN(TokenName, Spelling) TokenKind::TokenName,
#include "toolchain/lex/token_kind.def"
};
inline constexpr llvm::ArrayRef<TokenKind> TokenKind::PoundKeywordTokens =
    PoundKeywordTokensStorage;

}  // namespace TinySwift::Lex

// We use formatv primarily for diagnostics. In these cases, it's expected that
// the spelling in source code should be used.
template <>
struct llvm::format_provider<TinySwift::Lex::TokenKind> {
  static void format(const TinySwift::Lex::TokenKind& kind, raw_ostream& out,
                     StringRef /*style*/) {
    auto spelling = kind.fixed_spelling();
    if (!spelling.empty()) {
      out << spelling;
    } else {
      // Default to the name if there's no fixed spelling.
      out << kind;
    }
  }
};

#endif  // TINYSWIFT_TOOLCHAIN_LEX_TOKEN_KIND_H_
