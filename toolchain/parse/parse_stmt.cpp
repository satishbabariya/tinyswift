// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/diagnostics/diagnostic.h"
#include "toolchain/parse/context.h"
#include "toolchain/parse/node_kind.h"
#include "toolchain/parse/typed_nodes.h"

namespace TinySwift::Parse {

namespace {
TINYSWIFT_DIAGNOSTIC(ExpectedStatementExpr, Error,
                     "expected expression or statement");
}  // namespace

// Forward declarations from other parse files.
auto ParseExpr(Context& context) -> void;
auto ParsePattern(Context& context) -> void;
auto ParseTypeAnnotation(Context& context) -> void;
auto ParseDecl(Context& context) -> void;

// Forward declarations for functions in this file (called from other TUs).
auto ParseStatement(Context& context) -> void;
auto ParseCodeBlock(Context& context) -> void;

// Internal forward declarations.
static auto ParseConditionList(Context& context) -> void;
static auto IsAtDeclStart(Context& context) -> bool;

// Parses a code block: `{ statements... }`
auto ParseCodeBlock(Context& context) -> void {
  auto open = context.ConsumeChecked(Lex::TokenKind::OpenCurlyBrace);
  context.AddLeafNode(NodeKind::CodeBlockStart, open);

  while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
         !context.AtEndOfFile()) {
    ParseStatement(context);
  }

  auto close = context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
  context.AddNode(NodeKind::CodeBlock, close);
}

// Parses a `return` statement: `return expr?`
static auto ParseReturnStatement(Context& context) -> void {
  auto ret = context.Consume();  // 'return'
  context.AddLeafNode(NodeKind::ReturnStatementStart, ret);

  // Parse optional return value. If the next token is on a new line or is `}`,
  // there's no return value.
  if (!context.AtEndOfFile() &&
      context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
      !context.AtStartOfLine()) {
    ParseExpr(context);
  }

  context.AddNode(NodeKind::ReturnStatement, ret);
}

// Parses a `break` statement: `break label?`
static auto ParseBreakStatement(Context& context) -> void {
  auto brk = context.Consume();  // 'break'
  context.AddLeafNode(NodeKind::BreakStatementStart, brk);
  context.AddNode(NodeKind::BreakStatement, brk);
}

// Parses a `continue` statement: `continue label?`
static auto ParseContinueStatement(Context& context) -> void {
  auto cont = context.Consume();  // 'continue'
  context.AddLeafNode(NodeKind::ContinueStatementStart, cont);
  context.AddNode(NodeKind::ContinueStatement, cont);
}

// Parses a `throw` statement: `throw expr`
static auto ParseThrowStatement(Context& context) -> void {
  auto thr = context.Consume();  // 'throw'
  context.AddLeafNode(NodeKind::ThrowStatementStart, thr);
  ParseExpr(context);
  context.AddNode(NodeKind::ThrowStatement, thr);
}

// Parses a `fallthrough` statement.
static auto ParseFallthroughStatement(Context& context) -> void {
  auto ft = context.Consume();  // 'fallthrough'
  context.AddLeafNode(NodeKind::FallthroughStatement, ft);
}

// Parses a condition list for if/guard/while.
// Supports: expression conditions, optional binding (`let x = expr`), and
// comma-separated multiple conditions.
static auto ParseConditionList(Context& context) -> void {
  // Parse first condition.
  auto kind = context.Peek();

  // Optional binding: `let x = expr` or `var x = expr`
  if (kind == Lex::TokenKind::LetKeyword ||
      kind == Lex::TokenKind::VarKeyword) {
    auto binding = context.Consume();
    ParsePattern(context);
    if (context.Peek() == Lex::TokenKind::Equal) {
      context.Consume();
      ParseExpr(context);
    }
    context.AddNode(NodeKind::OptionalBindingCondition, binding);
  } else {
    // Expression condition.
    ParseExpr(context);
  }

  // Parse comma-separated additional conditions.
  while (context.Peek() == Lex::TokenKind::Comma) {
    auto comma = context.Consume();
    context.AddLeafNode(NodeKind::PatternListComma, comma);

    kind = context.Peek();
    if (kind == Lex::TokenKind::LetKeyword ||
        kind == Lex::TokenKind::VarKeyword) {
      auto binding = context.Consume();
      ParsePattern(context);
      if (context.Peek() == Lex::TokenKind::Equal) {
        context.Consume();
        ParseExpr(context);
      }
      context.AddNode(NodeKind::OptionalBindingCondition, binding);
    } else {
      ParseExpr(context);
    }
  }
}

// Parses an `if` statement: `if condition { } else { }` or `else if { }`
static auto ParseIfStatement(Context& context) -> void {
  auto if_token = context.Consume();  // 'if'

  // Parse the condition (bare expression, no parens required).
  ParseConditionList(context);
  context.AddNode(NodeKind::IfCondition, if_token);

  // Parse the then-block.
  ParseCodeBlock(context);

  // Parse optional else clause.
  if (context.Peek() == Lex::TokenKind::ElseKeyword) {
    auto else_token = context.Consume();
    context.AddLeafNode(NodeKind::IfStatementElse, else_token);

    if (context.Peek() == Lex::TokenKind::IfKeyword) {
      // `else if` chain.
      ParseIfStatement(context);
    } else {
      // `else { ... }`
      ParseCodeBlock(context);
    }
  }

  context.AddNode(NodeKind::IfStatement, if_token);
}

// Parses a `guard` statement: `guard condition else { }`
static auto ParseGuardStatement(Context& context) -> void {
  auto guard_token = context.Consume();  // 'guard'

  // Parse the condition.
  ParseConditionList(context);
  context.AddNode(NodeKind::GuardCondition, guard_token);

  // Expect `else`.
  auto else_token = context.ConsumeChecked(Lex::TokenKind::ElseKeyword);
  context.AddLeafNode(NodeKind::GuardStatementElse, else_token);

  // Parse the else-block (must contain a control transfer statement).
  ParseCodeBlock(context);

  context.AddNode(NodeKind::GuardStatement, guard_token);
}

// Parses a `while` statement: `while condition { }`
static auto ParseWhileStatement(Context& context) -> void {
  auto while_token = context.Consume();  // 'while'

  // Parse the condition (bare expression).
  ParseConditionList(context);
  context.AddNode(NodeKind::WhileCondition, while_token);

  // Parse the body.
  ParseCodeBlock(context);

  context.AddNode(NodeKind::WhileStatement, while_token);
}

// Parses a `for-in` statement: `for pattern in expr { }`
static auto ParseForInStatement(Context& context) -> void {
  auto for_token = context.Consume();  // 'for'
  context.AddLeafNode(NodeKind::ForInIntroducer, for_token);

  // Parse the pattern.
  ParsePattern(context);

  // Expect `in`.
  context.ConsumeChecked(Lex::TokenKind::InKeyword);

  // Parse the sequence expression.
  ParseExpr(context);

  // Optional where clause.
  if (context.Peek() == Lex::TokenKind::WhereKeyword) {
    context.Consume();
    ParseExpr(context);
  }

  // Parse the body.
  ParseCodeBlock(context);

  context.AddNode(NodeKind::ForInStatement, for_token);
}

// Parses a `repeat-while` statement: `repeat { } while condition`
static auto ParseRepeatWhileStatement(Context& context) -> void {
  auto repeat_token = context.Consume();  // 'repeat'

  // Parse the body.
  ParseCodeBlock(context);

  // Expect `while`.
  context.ConsumeChecked(Lex::TokenKind::WhileKeyword);

  // Parse the condition.
  ParseExpr(context);

  context.AddNode(NodeKind::RepeatWhileStatement, repeat_token);
}

// Parses a `switch` statement: `switch expr { case ...: ... }`
static auto ParseSwitchStatement(Context& context) -> void {
  auto switch_token = context.Consume();  // 'switch'
  context.AddLeafNode(NodeKind::SwitchIntroducer, switch_token);

  // Parse the subject expression.
  ParseExpr(context);

  // Expect `{`.
  context.ConsumeChecked(Lex::TokenKind::OpenCurlyBrace);

  // Parse cases.
  while (context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
         !context.AtEndOfFile()) {
    if (context.Peek() == Lex::TokenKind::CaseKeyword) {
      auto case_token = context.Consume();

      // Parse case patterns (comma-separated).
      ParsePattern(context);
      while (context.Peek() == Lex::TokenKind::Comma) {
        context.Consume();
        ParsePattern(context);
      }

      // Optional where clause.
      if (context.Peek() == Lex::TokenKind::WhereKeyword) {
        context.Consume();
        ParseExpr(context);
      }

      context.ConsumeChecked(Lex::TokenKind::Colon);
      context.AddNode(NodeKind::SwitchCaseLabel, case_token);

      // Parse case body (statements until next case/default/}).
      while (context.Peek() != Lex::TokenKind::CaseKeyword &&
             context.Peek() != Lex::TokenKind::DefaultKeyword &&
             context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
             !context.AtEndOfFile()) {
        ParseStatement(context);
      }
      context.AddNode(NodeKind::SwitchCaseBody, case_token);
    } else if (context.Peek() == Lex::TokenKind::DefaultKeyword) {
      auto default_token = context.Consume();
      context.ConsumeChecked(Lex::TokenKind::Colon);
      context.AddNode(NodeKind::SwitchDefaultLabel, default_token);

      // Parse default body.
      while (context.Peek() != Lex::TokenKind::CaseKeyword &&
             context.Peek() != Lex::TokenKind::DefaultKeyword &&
             context.Peek() != Lex::TokenKind::CloseCurlyBrace &&
             !context.AtEndOfFile()) {
        ParseStatement(context);
      }
      context.AddNode(NodeKind::SwitchCaseBody, default_token);
    } else {
      // Unexpected token in switch body.
      context.Consume();
    }
  }

  context.ConsumeChecked(Lex::TokenKind::CloseCurlyBrace);
  context.AddNode(NodeKind::SwitchStatement, switch_token);
}

// Parses a `do` statement: `do { } catch pattern { }`
static auto ParseDoStatement(Context& context) -> void {
  auto do_token = context.Consume();  // 'do'
  context.AddLeafNode(NodeKind::DoIntroducer, do_token);

  // Parse the do-block.
  ParseCodeBlock(context);

  // Parse catch clauses.
  while (context.Peek() == Lex::TokenKind::CatchKeyword) {
    auto catch_token = context.Consume();

    // Optional pattern.
    if (context.Peek() != Lex::TokenKind::OpenCurlyBrace) {
      ParsePattern(context);

      // Optional where clause.
      if (context.Peek() == Lex::TokenKind::WhereKeyword) {
        context.Consume();
        ParseExpr(context);
      }
    }

    // Parse catch body.
    ParseCodeBlock(context);
    context.AddNode(NodeKind::DoCatchClause, catch_token);
  }

  context.AddNode(NodeKind::DoStatement, do_token);
}

// M98: Parses a `yield` statement: `yield expr`
static auto ParseYieldStatement(Context& context) -> void {
  auto yield_token = context.Consume();  // 'yield'
  context.AddLeafNode(NodeKind::YieldStatementStart, yield_token);
  ParseExpr(context);
  context.AddNode(NodeKind::YieldStatement, yield_token);
}

// Parses a `defer` statement: `defer { ... }`
static auto ParseDeferStatement(Context& context) -> void {
  auto defer_token = context.Consume();  // 'defer'
  context.AddLeafNode(NodeKind::DeferStatementStart, defer_token);

  // Parse the deferred code block.
  ParseCodeBlock(context);

  context.AddNode(NodeKind::DeferStatement, defer_token);
}

// Determines if the current token could start a declaration.
static auto IsAtDeclStart(Context& context) -> bool {
  auto kind = context.Peek();
  return kind == Lex::TokenKind::LetKeyword ||
         kind == Lex::TokenKind::VarKeyword ||
         kind == Lex::TokenKind::FuncKeyword ||
         kind == Lex::TokenKind::StructKeyword ||
         kind == Lex::TokenKind::ClassKeyword ||
         kind == Lex::TokenKind::EnumKeyword ||
         kind == Lex::TokenKind::ProtocolKeyword ||
         kind == Lex::TokenKind::ExtensionKeyword ||
         kind == Lex::TokenKind::ImportKeyword ||
         kind == Lex::TokenKind::TypealiasKeyword ||
         kind == Lex::TokenKind::InitKeyword ||
         kind == Lex::TokenKind::DeinitKeyword ||
         kind == Lex::TokenKind::SubscriptKeyword ||
         kind == Lex::TokenKind::PublicKeyword ||
         kind == Lex::TokenKind::PrivateKeyword ||
         kind == Lex::TokenKind::InternalKeyword ||
         kind == Lex::TokenKind::FileprivateKeyword ||
         kind == Lex::TokenKind::StaticKeyword ||
         kind == Lex::TokenKind::At ||
         // M56: `mutating` is a contextual keyword (Identifier with text "mutating").
         (kind == Lex::TokenKind::Identifier &&
          context.GetTokenText() == "mutating") ||
         // M112: `comptime func` is a declaration.
         (kind == Lex::TokenKind::ComptimeKeyword &&
          context.PeekNext() == Lex::TokenKind::FuncKeyword);
}

// Parses a single statement or declaration.
auto ParseStatement(Context& context) -> void {
  auto kind = context.Peek();

  // Declarations.
  if (IsAtDeclStart(context)) {
    ParseDecl(context);
    return;
  }

  // Return statement.
  if (kind == Lex::TokenKind::ReturnKeyword) {
    ParseReturnStatement(context);
    return;
  }

  // Break statement.
  if (kind == Lex::TokenKind::BreakKeyword) {
    ParseBreakStatement(context);
    return;
  }

  // Continue statement.
  if (kind == Lex::TokenKind::ContinueKeyword) {
    ParseContinueStatement(context);
    return;
  }

  // Throw statement.
  if (kind == Lex::TokenKind::ThrowKeyword) {
    ParseThrowStatement(context);
    return;
  }

  // Fallthrough statement.
  if (kind == Lex::TokenKind::FallthroughKeyword) {
    ParseFallthroughStatement(context);
    return;
  }

  // If statement.
  if (kind == Lex::TokenKind::IfKeyword) {
    ParseIfStatement(context);
    return;
  }

  // Guard statement.
  if (kind == Lex::TokenKind::GuardKeyword) {
    ParseGuardStatement(context);
    return;
  }

  // While statement.
  if (kind == Lex::TokenKind::WhileKeyword) {
    ParseWhileStatement(context);
    return;
  }

  // For-in statement.
  if (kind == Lex::TokenKind::ForKeyword) {
    ParseForInStatement(context);
    return;
  }

  // Repeat-while statement.
  if (kind == Lex::TokenKind::RepeatKeyword) {
    ParseRepeatWhileStatement(context);
    return;
  }

  // Switch statement.
  if (kind == Lex::TokenKind::SwitchKeyword) {
    ParseSwitchStatement(context);
    return;
  }

  // Do statement.
  if (kind == Lex::TokenKind::DoKeyword) {
    ParseDoStatement(context);
    return;
  }

  // Defer statement.
  if (kind == Lex::TokenKind::DeferKeyword) {
    ParseDeferStatement(context);
    return;
  }

  // M98: Yield statement.
  if (kind == Lex::TokenKind::YieldKeyword) {
    ParseYieldStatement(context);
    return;
  }

  // M107: Conditional compilation.
  if (kind == Lex::TokenKind::PoundIf) {
    auto pound_if = context.Consume();
    context.AddLeafNode(NodeKind::PoundIfStart, pound_if);

    // Parse the condition identifier (e.g. `DEBUG`).
    if (context.Peek() == Lex::TokenKind::Identifier) {
      auto cond_token = context.Consume();
      context.AddLeafNode(NodeKind::IdentifierNameExpr, cond_token);
    }

    // Parse body until #else/#elseif/#endif.
    while (context.Peek() != Lex::TokenKind::PoundElse &&
           context.Peek() != Lex::TokenKind::PoundElseif &&
           context.Peek() != Lex::TokenKind::PoundEndif &&
           !context.AtEndOfFile()) {
      ParseStatement(context);
    }

    // Handle #else block.
    if (context.Peek() == Lex::TokenKind::PoundElse) {
      auto else_token = context.Consume();
      context.AddLeafNode(NodeKind::PoundElseDecl, else_token);
      while (context.Peek() != Lex::TokenKind::PoundEndif &&
             !context.AtEndOfFile()) {
        ParseStatement(context);
      }
    }

    // Handle #elseif (consume as else for now).
    if (context.Peek() == Lex::TokenKind::PoundElseif) {
      auto elseif_token = context.Consume();
      context.AddLeafNode(NodeKind::PoundElseifDecl, elseif_token);
      // Skip condition identifier.
      if (context.Peek() == Lex::TokenKind::Identifier) {
        context.Consume();
      }
      while (context.Peek() != Lex::TokenKind::PoundEndif &&
             !context.AtEndOfFile()) {
        ParseStatement(context);
      }
    }

    if (context.Peek() == Lex::TokenKind::PoundEndif) {
      auto endif_token = context.Consume();
      context.AddNode(NodeKind::PoundIfDecl, endif_token);
    }
    return;
  }

  // M108: Compile-time warning/error/assert directives.
  if (kind == Lex::TokenKind::PoundWarning) {
    auto token = context.Consume();
    // Expect `("message")`
    if (context.Peek() == Lex::TokenKind::OpenParen) {
      context.Consume();  // (
      if (context.Peek() == Lex::TokenKind::StringLiteral) {
        auto str_token = context.Consume();
        context.AddLeafNode(NodeKind::StringLiteral, str_token);
      }
      if (context.Peek() == Lex::TokenKind::CloseParen) {
        context.Consume();  // )
      }
    }
    context.AddNode(NodeKind::PoundWarningDecl, token);
    return;
  }

  if (kind == Lex::TokenKind::PoundError) {
    auto token = context.Consume();
    // Expect `("message")`
    if (context.Peek() == Lex::TokenKind::OpenParen) {
      context.Consume();  // (
      if (context.Peek() == Lex::TokenKind::StringLiteral) {
        auto str_token = context.Consume();
        context.AddLeafNode(NodeKind::StringLiteral, str_token);
      }
      if (context.Peek() == Lex::TokenKind::CloseParen) {
        context.Consume();  // )
      }
    }
    context.AddNode(NodeKind::PoundErrorDecl, token);
    return;
  }

  if (kind == Lex::TokenKind::PoundAssert) {
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::PoundAssertStart, token);
    // Expect `(expr)` or `(expr, "message")`
    if (context.Peek() == Lex::TokenKind::OpenParen) {
      context.Consume();  // (
      ParseExpr(context);
      // Optional message.
      if (context.Peek() == Lex::TokenKind::Comma) {
        context.Consume();  // ,
        if (context.Peek() == Lex::TokenKind::StringLiteral) {
          auto str_token = context.Consume();
          context.AddLeafNode(NodeKind::StringLiteral, str_token);
        }
      }
      auto close = context.ConsumeChecked(Lex::TokenKind::CloseParen);
      context.AddNode(NodeKind::PoundAssertDecl, close);
    }
    return;
  }

  // Expression statement (fallback).
  if (kind != Lex::TokenKind::FileEnd &&
      kind != Lex::TokenKind::CloseCurlyBrace) {
    auto start = context.position();
    ParseExpr(context);
    context.AddNode(NodeKind::ExprStatement, start);
    return;
  }

  // Skip unrecognized tokens to avoid infinite loops.
  if (!context.AtEndOfFile()) {
    context.EmitError(ExpectedStatementExpr);
    auto token = context.Consume();
    context.AddLeafNode(NodeKind::InvalidParse, token, true);
  }
}

}  // namespace TinySwift::Parse
