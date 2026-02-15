// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"

namespace TinySwift::Parse {

namespace {
TINYSWIFT_DIAGNOSTIC(ExpectedPattern, Error, "expected pattern");
}  // namespace

// Forward declarations from parse_type.cpp.
auto ParseTypeAnnotation(Context& context) -> void;

// Forward declaration from parse_expr.cpp.
auto ParseExpr(Context& context) -> void;

// Forward declaration for this file (called from other TUs).
auto ParsePattern(Context& context) -> void;

// Parses a pattern.
auto ParsePattern(Context& context) -> void {
  auto kind = context.Peek();

  // Wildcard pattern: `_`
  if (kind == Lex::TokenKind::UnderscoreKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::WildcardPattern, token);
    return;
  }

  // Identifier pattern: `name`
  if (kind == Lex::TokenKind::Identifier) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierPattern, token);

    // Handle type annotation: `name: Type`
    if (context.Peek() == Lex::TokenKind::Colon) {
      ParseTypeAnnotation(context);
    }
    return;
  }

  // Binding pattern: `let x` / `var x` in pattern context
  if (kind == Lex::TokenKind::LetKeyword ||
      kind == Lex::TokenKind::VarKeyword) {
    context.Consume();
    ParsePattern(context);
    // The binding keyword wraps around the inner pattern.
    return;
  }

  // `is Type` pattern
  if (kind == Lex::TokenKind::IsKeyword) {
    auto is_token = context.Consume();
    // Parse the type after `is`.
    if (context.Peek() == Lex::TokenKind::Identifier ||
        context.Peek() == Lex::TokenKind::AnyKeyword ||
        context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
      auto type_name = context.Consume();
      context.AddLeafNode(NodeKind::IdentifierType, type_name);
    }
    context.AddNode(NodeKind::IsPattern, is_token);
    return;
  }

  // Enum case pattern: `.case(associated)`
  if (kind == Lex::TokenKind::Period) {
    auto dot = context.Consume();
    if (context.Peek() == Lex::TokenKind::Identifier) {
      context.Consume();  // case name
    }
    context.AddLeafNode(NodeKind::EnumCasePattern, dot);
    return;
  }

  // Tuple pattern: `(x, y)`
  if (kind == Lex::TokenKind::OpenParen) {
    auto open = context.Consume();
    if (context.Peek() != Lex::TokenKind::CloseParen) {
      ParsePattern(context);
      while (context.Peek() == Lex::TokenKind::Comma) {
        context.Consume();
        ParsePattern(context);
      }
    }
    context.ConsumeChecked(Lex::TokenKind::CloseParen);
    context.AddNode(NodeKind::TuplePattern, open);
    return;
  }

  // Expression pattern (fallback): use expression parser.
  if (kind != Lex::TokenKind::FileEnd &&
      kind != Lex::TokenKind::CloseCurlyBrace) {
    ParseExpr(context);
    context.AddNode(NodeKind::ExprPattern, context.position());
    return;
  }

  context.EmitError(ExpectedPattern);
  context.AddLeafNode(NodeKind::InvalidParse, context.position(), true);
}

}  // namespace TinySwift::Parse
