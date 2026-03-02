// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"

namespace TinySwift::Parse {

namespace {
TINYSWIFT_DIAGNOSTIC(ExpectedExprParser, Error, "expected expression");
}  // namespace

// Forward declarations from other parse files.
auto ParseTypeAnnotation(Context& context) -> void;
auto ParseCodeBlock(Context& context) -> void;
auto ParseStatement(Context& context) -> void;
auto ParseGenericArguments(Context& context) -> void;

// Forward declaration for this file (called from other TUs).
auto ParseExpr(Context& context) -> void;

// Internal forward declarations.
static auto ParsePrimaryExpr(Context& context) -> void;
static auto ParsePostfixExpr(Context& context) -> void;
static auto ParseExprImpl(Context& context, int min_precedence) -> void;
static auto ParsePrimaryType(Context& context) -> void;

// Swift operator precedence levels (high to low priority = high number).
// Higher number means higher precedence (binds tighter).
enum Precedence {
  Prec_None = 0,
  Prec_Assignment = 1,   // =, +=, -=, etc.
  Prec_Ternary = 2,      // ? :
  Prec_LogicalOr = 3,    // ||
  Prec_LogicalAnd = 4,   // &&
  Prec_NilCoalescing = 5, // ??
  Prec_Comparison = 6,   // ==, !=, <, >, <=, >=
  Prec_Range = 7,        // ..<, ...
  Prec_Addition = 8,     // +, -
  Prec_Multiplication = 9, // *, /, %
  Prec_BitwiseShift = 10,  // <<, >>
  Prec_Cast = 11,        // as, as?, as!, is
  Prec_Prefix = 12,      // -, !, ~
  Prec_Postfix = 13,     // .member, (args), [sub], !, ?
};

// Get the precedence for a binary operator token.
static auto GetBinaryPrecedence(Context& context) -> int {
  auto kind = context.Peek();
  auto text = context.GetTokenText();

  // Fixed-token binary operators.
  switch (kind) {
    case Lex::TokenKind::Equal:
      return Prec_Assignment;
    case Lex::TokenKind::EqualEqual:
    case Lex::TokenKind::ExclaimEqual:
    case Lex::TokenKind::LessEqual:
    case Lex::TokenKind::GreaterEqual:
      return Prec_Comparison;
    case Lex::TokenKind::EllipsisToken:
      return Prec_Range;  // `...` range operator.
    case Lex::TokenKind::MinusGreater:
      return Prec_None;  // Not a binary operator in expression context.
    case Lex::TokenKind::Amp:
      return Prec_Addition;  // Bitwise AND: same precedence bucket as |, ^.
    case Lex::TokenKind::Question:
      return Prec_Ternary;
    case Lex::TokenKind::QuestionQuestion:
      return Prec_NilCoalescing;
    case Lex::TokenKind::AsKeyword:
      return Prec_Cast;
    case Lex::TokenKind::IsKeyword:
      return Prec_Cast;
    default:
      break;
  }

  // Spacing-classified binary operators.
  if (kind == Lex::TokenKind::OperatorBinarySpaced ||
      kind == Lex::TokenKind::OperatorBinaryUnspaced) {
    if (text == "+") return Prec_Addition;
    if (text == "-") return Prec_Addition;
    if (text == "*") return Prec_Multiplication;
    if (text == "/") return Prec_Multiplication;
    if (text == "%") return Prec_Multiplication;
    if (text == "&&") return Prec_LogicalAnd;
    if (text == "||") return Prec_LogicalOr;
    if (text == "??") return Prec_NilCoalescing;
    if (text == "..<") return Prec_Range;
    if (text == "...") return Prec_Range;
    if (text == "<") return Prec_Comparison;
    if (text == ">") return Prec_Comparison;
    if (text == "<<") return Prec_BitwiseShift;
    if (text == ">>") return Prec_BitwiseShift;
    if (text == "+=") return Prec_Assignment;
    if (text == "-=") return Prec_Assignment;
    if (text == "*=") return Prec_Assignment;
    if (text == "/=") return Prec_Assignment;
    if (text == "%=") return Prec_Assignment;
    if (text == "&=") return Prec_Assignment;
    if (text == "|=") return Prec_Assignment;
    if (text == "^=") return Prec_Assignment;
    // Unrecognized binary operator — default to addition precedence.
    // This handles user-defined operators (M58) which don't have explicit
    // precedence groups yet.  A full implementation would look up the
    // precedence group from the operator declaration.
    return Prec_Addition;
  }

  return Prec_None;
}

// Public entry point for expression parsing.
auto ParseExpr(Context& context) -> void {
  ParseExprImpl(context, Prec_Assignment);
}

// Pratt parser core.
static auto ParseExprImpl(Context& context, int min_precedence) -> void {
  // Parse prefix operators.
  auto kind = context.Peek();

  // `try`, `try?`, `try!`
  if (kind == Lex::TokenKind::TryKeyword) {
    auto try_token = context.Consume();
    auto next = context.Peek();
    if (next == Lex::TokenKind::Question) {
      context.Consume();
      ParseExprImpl(context, Prec_Prefix);
      context.AddNode(NodeKind::TryQuestionExpr, try_token);
      return;
    }
    if (next == Lex::TokenKind::Exclaim) {
      context.Consume();
      ParseExprImpl(context, Prec_Prefix);
      context.AddNode(NodeKind::TryExclaimExpr, try_token);
      return;
    }
    ParseExprImpl(context, Prec_Prefix);
    context.AddNode(NodeKind::TryExpr, try_token);
    return;
  }

  // M100: `await <expr>` prefix expression.
  if (kind == Lex::TokenKind::AwaitKeyword) {
    auto await_token = context.Consume();
    ParseExprImpl(context, Prec_Prefix);
    context.AddNode(NodeKind::AwaitExpr, await_token);
    return;
  }

  // M112: `comptime <expr>` prefix expression.
  // Uses Prec_Ternary so that binary operators (arithmetic, comparison,
  // logical) are included in the comptime operand.
  if (kind == Lex::TokenKind::ComptimeKeyword) {
    auto comptime_token = context.Consume();
    ParseExprImpl(context, Prec_Ternary);
    context.AddNode(NodeKind::ComptimeExpr, comptime_token);
    return;
  }

  // M40: Address-of expression `&varName` (prefix `&`, not binary bitwise-AND).
  if (kind == Lex::TokenKind::Amp) {
    auto amp_token = context.Consume();
    ParseExprImpl(context, Prec_Prefix);
    context.AddNode(NodeKind::AddressOfExpr, amp_token);
  // Prefix operators: -, !, ~
  } else if (kind == Lex::TokenKind::OperatorPrefix) {
    auto op = context.Consume();
    ParseExprImpl(context, Prec_Prefix);
    context.AddNode(NodeKind::PrefixOperatorExpr, op);
  // Prefix `!` — the lexer always emits Exclaim for lone `!`; when it
  // appears at expression-start (with leading whitespace or at BOF) it is
  // the logical-NOT prefix operator.
  } else if (kind == Lex::TokenKind::Exclaim) {
    auto op = context.Consume();
    ParseExprImpl(context, Prec_Prefix);
    context.AddNode(NodeKind::PrefixOperatorExpr, op);
  } else {
    ParsePrimaryExpr(context);
    ParsePostfixExpr(context);
  }

  // Now parse binary operators using Pratt parsing.
  while (true) {
    int prec = GetBinaryPrecedence(context);
    if (prec < min_precedence || prec == Prec_None) {
      break;
    }

    kind = context.Peek();

    // Ternary operator: `? then_expr : else_expr`
    if (kind == Lex::TokenKind::Question && prec == Prec_Ternary) {
      auto q_token = context.Consume();
      ParseExpr(context);  // then expression (full precedence)
      context.ConsumeChecked(Lex::TokenKind::Colon);
      ParseExprImpl(context, Prec_Ternary);  // else expression
      context.AddNode(NodeKind::TernaryExpr, q_token);
      continue;
    }

    // Assignment: `=`
    if (kind == Lex::TokenKind::Equal) {
      auto eq_token = context.Consume();
      ParseExprImpl(context, Prec_Assignment);  // Right-associative.
      context.AddNode(NodeKind::AssignmentExpr, eq_token);
      continue;
    }

    // `as`, `as?`, `as!` casts.
    if (kind == Lex::TokenKind::AsKeyword) {
      auto as_token = context.Consume();
      auto next_kind = context.Peek();
      if (next_kind == Lex::TokenKind::Question) {
        context.Consume();
        // Parse the type.
        ParsePrimaryType(context);
        context.AddNode(NodeKind::AsQuestionExpr, as_token);
      } else if (next_kind == Lex::TokenKind::Exclaim) {
        context.Consume();
        ParsePrimaryType(context);
        context.AddNode(NodeKind::AsExclaimExpr, as_token);
      } else {
        ParsePrimaryType(context);
        context.AddNode(NodeKind::AsExpr, as_token);
      }
      continue;
    }

    // `is` type check.
    if (kind == Lex::TokenKind::IsKeyword) {
      auto is_token = context.Consume();
      ParsePrimaryType(context);
      context.AddNode(NodeKind::IsExpr, is_token);
      continue;
    }

    // Regular binary operator.
    auto op_token = context.Consume();
    // Right-associative for assignment-like operators.
    int next_min = (prec == Prec_Assignment || prec == Prec_NilCoalescing)
                       ? prec
                       : prec + 1;
    ParseExprImpl(context, next_min);
    context.AddNode(NodeKind::InfixOperatorExpr, op_token);
  }
}

// Parses a primary (atomic) expression.
static auto ParsePrimaryExpr(Context& context) -> void {
  auto kind = context.Peek();

  // Integer literal.
  if (kind == Lex::TokenKind::IntegerLiteral) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::IntLiteral, token);
    return;
  }

  // Floating-point literal.
  if (kind == Lex::TokenKind::FloatingLiteral) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::FloatingLiteral, token);
    return;
  }

  // String literal.
  if (kind == Lex::TokenKind::StringLiteral) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::StringLiteral, token);
    return;
  }

  // Boolean literals.
  if (kind == Lex::TokenKind::TrueKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::BoolLiteralTrue, token);
    return;
  }
  if (kind == Lex::TokenKind::FalseKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::BoolLiteralFalse, token);
    return;
  }

  // Nil literal.
  if (kind == Lex::TokenKind::NilKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::NilLiteral, token);
    return;
  }

  // Identifier expression.
  if (kind == Lex::TokenKind::Identifier) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierNameExpr, token);
    return;
  }

  // `self`
  if (kind == Lex::TokenKind::SelfKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::SelfExpr, token);
    return;
  }

  // `super`
  if (kind == Lex::TokenKind::SuperKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::SuperExpr, token);
    return;
  }

  // Dollar identifier ($0, $1, etc.)
  if (kind == Lex::TokenKind::DollarIdent) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::DollarIdentExpr, token);
    return;
  }

  // Parenthesized expression: `(expr)`
  if (kind == Lex::TokenKind::OpenParen) {
    auto open = context.Consume();
    context.AddLeafNode(NodeKind::ParenExprStart, open);
    ParseExpr(context);

    // Check for tuple: multiple comma-separated elements.
    if (context.Peek() == Lex::TokenKind::Comma) {
      // This is a tuple expression.
      while (context.Peek() == Lex::TokenKind::Comma) {
        auto comma = context.Consume();
        context.AddLeafNode(NodeKind::PatternListComma, comma);
        ParseExpr(context);
      }
      auto close = context.ConsumeChecked(Lex::TokenKind::CloseParen);
      context.AddNode(NodeKind::TupleExpr, close);
    } else {
      auto close = context.ConsumeChecked(Lex::TokenKind::CloseParen);
      context.AddNode(NodeKind::ParenExpr, close);
    }
    return;
  }

  // Array or dictionary literal: `[expr, ...]` or `[key: value, ...]`
  // Use 1-token lookahead: if the SECOND token after `[` is `:`, it's a dict.
  if (kind == Lex::TokenKind::OpenSquareBracket) {
    auto open = context.Consume();

    // Determine array vs dictionary before emitting the bracket start node.
    // For dictionary: second token after `[` is Colon (e.g. `["a": 1, ...]`).
    bool is_dict = (context.Peek() != Lex::TokenKind::CloseSquareBracket) &&
                   (context.PeekNext() == Lex::TokenKind::Colon);

    // M91: Empty dictionary literal `[:]`.
    if (context.Peek() == Lex::TokenKind::Colon &&
        context.PeekNext() == Lex::TokenKind::CloseSquareBracket) {
      context.AddLeafNode(NodeKind::DictionaryExprStart, open);
      context.Consume();  // ':'
      auto close = context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
      context.AddNode(NodeKind::DictionaryExpr, close);
      return;
    }

    if (is_dict) {
      // Dictionary literal: `[key: value, ...]`
      context.AddLeafNode(NodeKind::DictionaryExprStart, open);

      // Parse first key:value entry.
      ParseExpr(context);  // key
      auto colon = context.ConsumeChecked(Lex::TokenKind::Colon);
      ParseExpr(context);  // value
      context.AddNode(NodeKind::DictionaryExprEntry, colon);

      while (context.Peek() == Lex::TokenKind::Comma) {
        auto comma = context.Consume();
        context.AddLeafNode(NodeKind::PatternListComma, comma);
        ParseExpr(context);  // key
        auto colon2 = context.ConsumeChecked(Lex::TokenKind::Colon);
        ParseExpr(context);  // value
        context.AddNode(NodeKind::DictionaryExprEntry, colon2);
      }
      auto close = context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
      context.AddNode(NodeKind::DictionaryExpr, close);
      return;
    }

    // Array literal: `[expr, expr, ...]`
    context.AddLeafNode(NodeKind::ArrayExprStart, open);
    if (context.Peek() != Lex::TokenKind::CloseSquareBracket) {
      ParseExpr(context);
      while (context.Peek() == Lex::TokenKind::Comma) {
        auto comma = context.Consume();
        context.AddLeafNode(NodeKind::PatternListComma, comma);
        ParseExpr(context);
      }
    }
    auto close = context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
    context.AddNode(NodeKind::ArrayExpr, close);
    return;
  }

  // Closure expression: `{ ... }` or `{ param in ... }`
  if (kind == Lex::TokenKind::OpenCurlyBrace) {
    auto open = context.Consume();
    context.AddLeafNode(NodeKind::ClosureExprStart, open);

    // Check for closure signature: `identifier in`
    if (context.Peek() == Lex::TokenKind::Identifier &&
        context.PeekNext() == Lex::TokenKind::InKeyword) {
      auto param_token = context.Consume();  // identifier (param name)
      context.AddLeafNode(NodeKind::ClosureParam, param_token);
      auto in_token = context.Consume();  // 'in'
      context.AddNode(NodeKind::ClosureSignature, in_token);
    }

    // Parse the body as statements until `}`.
    while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
           !context.AtEndOfFile()) {
      ParseStatement(context);
    }

    auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
    context.AddNode(NodeKind::ClosureExpr, close);
    return;
  }

  // Key path expression: `\Type.path`
  if (kind == Lex::TokenKind::Backslash) {
    auto backslash = context.Consume();
    context.AddLeafNode(NodeKind::KeyPathExpr, backslash);
    return;
  }

  // M107: Source location directives as expressions.
  if (kind == Lex::TokenKind::PoundFile) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::PoundFileExpr, token);
    return;
  }
  if (kind == Lex::TokenKind::PoundLine) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::PoundLineExpr, token);
    return;
  }
  if (kind == Lex::TokenKind::PoundColumn) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::PoundColumnExpr, token);
    return;
  }
  if (kind == Lex::TokenKind::PoundFunction) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::PoundFunctionExpr, token);
    return;
  }

  // If nothing matched, this is an error.
  context.EmitError(ExpectedExprParser);
  auto err_token = context.Consume();
  context.AddLeafNode(NodeKind::InvalidParse, err_token, true);
}

// Parses postfix expressions: `.member`, `(args)`, `[subscript]`, `!`, `?`
static auto ParsePostfixExpr(Context& context) -> void {
  while (true) {
    auto kind = context.Peek();

    // Member access: `.member`
    if (kind == Lex::TokenKind::Period) {
      auto dot = context.Consume();
      if (context.Peek() == Lex::TokenKind::Identifier) {
        auto member = context.Consume();
        context.AddLeafNode(NodeKind::IdentifierNameExpr, member);
        context.AddNode(NodeKind::MemberAccessExpr, dot);
      } else if (context.Peek() == Lex::TokenKind::IntegerLiteral) {
        // Tuple member access: `.0`, `.1`
        auto member = context.Consume();
        context.AddLeafNode(NodeKind::IntLiteral, member);
        context.AddNode(NodeKind::MemberAccessExpr, dot);
      } else if (context.Peek() == Lex::TokenKind::SelfKeyword) {
        auto self_token = context.Consume();
        context.AddLeafNode(NodeKind::SelfExpr, self_token);
        context.AddNode(NodeKind::MemberAccessExpr, dot);
      }
      continue;
    }

    // M66: Generic arguments in call expression: `identity<Int>(42)`.
    // Detect `<` as OperatorBinaryUnspaced (left-bound to identifier, right-bound
    // to type name) — distinct from comparison `a < b` (OperatorBinarySpaced).
    if ((kind == Lex::TokenKind::OperatorBinaryUnspaced ||
         kind == Lex::TokenKind::OperatorPrefix) &&
        context.GetTokenText() == "<") {
      ParseGenericArguments(context);
      // M80: If followed by `.`, wrap base+generic_args into GenericSpecExpr
      // so MemberAccessExpr (child_count=2) captures the whole type as one child.
      if (context.Peek() == Lex::TokenKind::Period) {
        context.AddNode(NodeKind::GenericSpecExpr,
                        Lex::TokenIndex(context.position().index - 1));
      }
      // After parsing generic args, re-evaluate from top of postfix loop
      // so `.member` check at the top can handle the Period token.
      continue;
    }

    // Call expression: `(args...)`
    // Only parse as call if there's no whitespace before the paren
    // (to distinguish from binary operator grouping).
    if (kind == Lex::TokenKind::OpenParen &&
        !context.tokens().HasLeadingWhitespace(context.position())) {
      auto open = context.Consume();
      context.AddNode(NodeKind::CallExprStart, open);

      // Parse comma-separated arguments, supporting `label: expr` syntax.
      if (context.Peek() != Lex::TokenKind::CloseParen) {
        // First argument: check for optional `label:` prefix.
        if (context.Peek() == Lex::TokenKind::Identifier &&
            context.PeekNext() == Lex::TokenKind::Colon) {
          auto label_token = context.Consume();
          context.AddLeafNode(NodeKind::ArgumentLabel, label_token);
          context.Consume();  // colon
        }
        ParseExpr(context);
        while (context.Peek() == Lex::TokenKind::Comma) {
          auto comma = context.Consume();
          context.AddLeafNode(NodeKind::PatternListComma, comma);
          // Subsequent argument: check for optional `label:` prefix.
          if (context.Peek() == Lex::TokenKind::Identifier &&
              context.PeekNext() == Lex::TokenKind::Colon) {
            auto label_token = context.Consume();
            context.AddLeafNode(NodeKind::ArgumentLabel, label_token);
            context.Consume();  // colon
          }
          ParseExpr(context);
        }
      }

      auto close = context.ConsumeChecked(Lex::TokenKind::CloseParen);
      context.AddNode(NodeKind::CallExpr, close);
      continue;
    }

    // Subscript expression: `[args]`
    if (kind == Lex::TokenKind::OpenSquareBracket &&
        !context.tokens().HasLeadingWhitespace(context.position())) {
      auto open = context.Consume();
      context.AddNode(NodeKind::SubscriptExprStart, open);

      if (context.Peek() != Lex::TokenKind::CloseSquareBracket) {
        ParseExpr(context);
        while (context.Peek() == Lex::TokenKind::Comma) {
          auto comma = context.Consume();
          context.AddLeafNode(NodeKind::PatternListComma, comma);
          ParseExpr(context);
        }
      }

      auto close = context.ConsumeChecked(
          Lex::TokenKind::CloseSquareBracket);
      context.AddNode(NodeKind::SubscriptExpr, close);
      continue;
    }

    // Postfix `!` (force unwrap) and `?` (optional chain).
    if (kind == Lex::TokenKind::OperatorPostfix) {
      auto text = context.GetTokenText();
      if (text == "!" || text == "?") {
        auto op = context.Consume();
        context.AddNode(NodeKind::PostfixOperatorExpr, op);
        continue;
      }
    }

    // Postfix `!` as Exclaim token (when not spacing-classified).
    if (kind == Lex::TokenKind::Exclaim &&
        !context.tokens().HasLeadingWhitespace(context.position())) {
      auto op = context.Consume();
      context.AddNode(NodeKind::PostfixOperatorExpr, op);
      continue;
    }

    // M60: Postfix `?` as Question token when followed by `.` (optional chaining `?.`).
    if (kind == Lex::TokenKind::Question &&
        context.PeekNext() == Lex::TokenKind::Period) {
      auto op = context.Consume();
      context.AddNode(NodeKind::PostfixOperatorExpr, op);
      continue;
    }

    break;
  }
}

// Helper: parse a primary type for use in cast expressions.
// This is a simplified version that just parses an identifier type.
static auto ParsePrimaryType(Context& context) -> void {
  auto kind = context.Peek();
  if (kind == Lex::TokenKind::Identifier ||
      kind == Lex::TokenKind::AnyKeyword ||
      kind == Lex::TokenKind::CapitalSelfKeyword) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::IdentifierType, token);

    // Handle optional postfix: `Type?`, `Type!`
    while (context.Peek() == Lex::TokenKind::Question) {
      auto q = context.Consume();
      context.AddNode(NodeKind::OptionalType, q);
    }
    return;
  }

  // Array type shorthand: `[Type]`
  if (kind == Lex::TokenKind::OpenSquareBracket) {
    auto open = context.Consume();
    ParsePrimaryType(context);
    context.ConsumeChecked(Lex::TokenKind::CloseSquareBracket);
    context.AddNode(NodeKind::ArrayType, open);
    return;
  }

  context.EmitError(ExpectedExprParser);
  auto token = context.Consume();
  context.AddLeafNode(NodeKind::InvalidParse, token, true);
}

}  // namespace TinySwift::Parse
