// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"

namespace TinySwift::Parse {

namespace {
TINYSWIFT_DIAGNOSTIC(ExpectedType, Error, "expected type");
}  // namespace

// Forward declarations for functions in this file that are called from other
// translation units.
auto ParseTypeAnnotation(Context& context) -> void;
auto ParseGenericArguments(Context& context) -> void;
auto ParseGenericParameterClause(Context& context) -> void;
auto ParseGenericWhereClause(Context& context) -> void;

// Forward declaration for this file (called from other TUs).
auto ParseType(Context& context) -> void;

// Internal forward declarations.
static auto ParsePrimaryType(Context& context) -> void;
static auto ParsePostfixType(Context& context) -> void;

// Parses generic arguments: `<Int, String>`
auto ParseGenericArguments(Context& context) -> void {
  // Assumes the current token is `<` (an operator token).
  auto open = context.Consume();
  context.AddLeafNode(NodeKind::GenericArgumentClauseStart, open);

  ParseType(context);
  while (context.Peek() == Lex::TokenKind::Comma) {
    auto comma = context.Consume();
    context.AddLeafNode(NodeKind::PatternListComma, comma);
    ParseType(context);
  }

  // Expect `>` (also an operator token).
  auto close = context.Consume();
  context.AddNode(NodeKind::GenericArgumentClause, close);
}

// Parses a type annotation: `: Type`
auto ParseTypeAnnotation(Context& context) -> void {
  auto colon = context.ConsumeChecked(Lex::TokenKind::Colon);
  ParseType(context);
  context.AddNode(NodeKind::TypeAnnotation, colon);
}

// Parses a type expression.
auto ParseType(Context& context) -> void {
  ParsePrimaryType(context);
  ParsePostfixType(context);

  // Handle function types: `(Type) -> Type`
  if (context.Peek() == Lex::TokenKind::MinusGreater) {
    auto arrow = context.Consume();
    ParseType(context);
    context.AddNode(NodeKind::FunctionType, arrow);
    return;
  }

  // Handle protocol composition: `Type & Type`
  if (context.Peek() == Lex::TokenKind::Amp) {
    auto amp = context.Consume();
    ParseType(context);
    context.AddNode(NodeKind::ProtocolCompositionType, amp);
    return;
  }
}

// Parses a primary type (the base before postfix modifiers).
static auto ParsePrimaryType(Context& context) -> void {
  auto kind = context.Peek();

  // M40: `inout Type` — emit InoutMarker leaf then parse actual type.
  if (kind == Lex::TokenKind::InoutKeyword) {
    auto inout_token = context.Consume();
    context.AddLeafNode(NodeKind::InoutMarker, inout_token);
    ParsePrimaryType(context);
    return;
  }

  // Array type: `[Type]` or dictionary type: `[Key: Value]`
  if (kind == Lex::TokenKind::OpenSquareBracket) {
    auto open = context.Consume();
    ParseType(context);

    if (context.Peek() == Lex::TokenKind::Colon) {
      // Dictionary type: `[Key: Value]`
      context.Consume();  // ':'
      ParseType(context);
      context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
      context.AddNode(NodeKind::DictionaryType, open);
    } else {
      // Array type: `[Type]`
      context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
      context.AddNode(NodeKind::ArrayType, open);
    }
    return;
  }

  // Tuple type: `(Type, Type)`
  if (kind == Lex::TokenKind::OpenParen) {
    auto open = context.Consume();
    if (context.Peek() != Lex::TokenKind::CloseParen) {
      ParseType(context);
      while (context.Peek() == Lex::TokenKind::Comma) {
        context.Consume();
        ParseType(context);
      }
    }
    context.ConsumeChecked(Lex::TokenKind::CloseParen);
    context.AddNode(NodeKind::TupleType, open);
    return;
  }

  // Identifier type: `Int`, `String`, `MyType`, `Any`, `Self`
  if (kind == Lex::TokenKind::Identifier ||
      kind == Lex::TokenKind::AnyKeyword ||
      kind == Lex::TokenKind::CapitalSelfKeyword) {
    auto name = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierType, name);

    // Handle generic arguments: `Type<T, U>`
    if (context.Peek() == Lex::TokenKind::OperatorBinaryUnspaced ||
        context.Peek() == Lex::TokenKind::OperatorPrefix) {
      auto text = context.GetTokenText();
      if (text == "<") {
        ParseGenericArguments(context);
      }
    }
    return;
  }

  // If we reach here, we couldn't parse a type.
  context.EmitError(ExpectedType);
  context.AddLeafNode(NodeKind::InvalidParse, context.position(), true);
}

// Parses postfix type modifiers: `?`, `!`, `.Member`
static auto ParsePostfixType(Context& context) -> void {
  while (true) {
    auto kind = context.Peek();

    // Optional type: `Type?`
    if (kind == Lex::TokenKind::Question) {
      auto q = context.Consume();
      context.AddNode(NodeKind::OptionalType, q);
      continue;
    }

    // Implicitly unwrapped optional type: `Type!`
    if (kind == Lex::TokenKind::Exclaim) {
      auto e = context.Consume();
      context.AddNode(NodeKind::ImplicitlyUnwrappedOptionalType, e);
      continue;
    }

    // Member type: `Type.Member`
    if (kind == Lex::TokenKind::Period) {
      auto dot = context.Consume();
      if (context.Peek() == Lex::TokenKind::Identifier ||
          context.Peek() == Lex::TokenKind::CapitalSelfKeyword) {
        auto member = context.Consume();
        context.AddLeafNode(NodeKind::IdentifierType, member);
        context.AddNode(NodeKind::MemberType, dot);
      }
      continue;
    }

    break;
  }
}

// Parses a generic parameter clause: `<T, U: Equatable>`
auto ParseGenericParameterClause(Context& context) -> void {
  auto open = context.Consume();
  context.AddLeafNode(NodeKind::GenericParameterClauseStart, open);

  // Parse first parameter.
  auto param_token = context.Consume();
  context.AddLeafNode(NodeKind::GenericParameter, param_token);

  // M66: Handle optional constraint `: Protocol` after parameter name.
  if (context.Peek() == Lex::TokenKind::Colon) {
    context.Consume();  // ':'
    ParseType(context);  // emits constraint IdentifierType as sibling
  }

  while (context.Peek() == Lex::TokenKind::Comma) {
    auto comma = context.Consume();
    context.AddLeafNode(NodeKind::PatternListComma, comma);
    auto next_param = context.Consume();
    context.AddLeafNode(NodeKind::GenericParameter, next_param);

    // M66: Handle optional constraint for subsequent parameters.
    if (context.Peek() == Lex::TokenKind::Colon) {
      context.Consume();  // ':'
      ParseType(context);  // emits constraint IdentifierType as sibling
    }
  }

  // Expect `>`.
  auto close = context.Consume();
  context.AddNode(NodeKind::GenericParameterClause, close);
}

// Parses a where clause: `where T: Equatable, U == Int`
auto ParseGenericWhereClause(Context& context) -> void {
  auto where_token = context.ConsumeChecked(Lex::TokenKind::WhereKeyword);
  // Simplified: just consume the where keyword and emit the node.
  context.AddLeafNode(NodeKind::GenericWhereClause, where_token);
}

}  // namespace TinySwift::Parse
